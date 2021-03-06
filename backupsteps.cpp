/**
 * RANKS 5/1969 after all night!!
 **/

#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <chrono>
#include <deque>

#define NEUTRAL -1
#define TREATED -2

using namespace std;
using namespace std::chrono;

class Grid;

int countFollow;
int countAttemp;
int countFill;
int countMove;

int nbPlayers;

int countExpansions;
bool timeout;
int stepSize;
high_resolution_clock::time_point roundStart;

int depth;

void checkTimeOut(){
    high_resolution_clock::time_point end = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(end - roundStart);
    //fprintf(stderr,"%.3f\n",time_span.count());
    if (time_span.count() >0.095) {
        timeout = true;
    }
    if (time_span.count() > 0.05 && stepSize == 0 ||
        time_span.count() > 0.07 && stepSize == 1    ) {
        //accelerationLevel++;
//        fprintf(stderr,"ACCELERATE: %d Depth:%d  Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",
//                accelerationLevel,depth,countPaths,countFill,(float)countFill/(float)countPaths*(float)100);
    }
}

/* Profiling feature */

//#define PROFILE

#ifdef PROFILE
    double timeSpent[10];
    #define FS high_resolution_clock::time_point functionStart = high_resolution_clock::now();
    #define FE(i) duration<double> time_span = duration_cast<duration<double>>(high_resolution_clock::now() - functionStart);\
                  timeSpent[i] += time_span.count();
#else
    #define FS
    #define FE(i)
#endif


enum Strategy {
    AssumeNoAgression, AssumeWorst
};

Strategy strategy = AssumeNoAgression;

#define GRID_CACHE_SIZE 10000

class GridCache;

class GridFactory {
public:
    std::vector<GridCache *> cache;
    int current;
    GridFactory();
    Grid *getNewGrid();
    void reset();
    int capacity();
    int size();
};

GridFactory gridFactory;

enum Direction {
    Up,Right,Down,Left,UpRight,DownRight,DownLeft,UpLeft,None
};
Direction around[8] = {UpLeft, Up, UpRight, Right, DownRight, Down, DownLeft, Left};

Direction &operator++(Direction &dir){
    dir = (Direction)(dir + 1);
    return dir;
}

void print(Direction dir) {
    switch(dir) {
    case Up:
        fprintf(stderr,"^\n|\n");
        break;
    case UpLeft:
        fprintf(stderr,"<\n \\\n");
        break;
    case UpRight:
        fprintf(stderr," >\n/\n");
        break;
    case Down:
        fprintf(stderr,"|\nv\n");
        break;
    case DownLeft:
        fprintf(stderr," />\n<\n");
        break;
    case DownRight:
        fprintf(stderr,"\\\n>\n");
        break;
    case Left:
        fprintf(stderr,"<-\n");
        break;
    case Right:
        fprintf(stderr,"->\n");
        break;
    case None:
        fprintf(stderr,"<>\n");
        break;
    default:
        fprintf(stderr,"???\n");
        break;
    }
}
void printTurn(Direction dir) {
    switch(dir) {
    case Left:
        fprintf(stderr,"<-\n |\n");
        break;
    case Right:
        fprintf(stderr,"->\n|\n");
        break;
    default:
        fprintf(stderr,"???\n");
        break;
    }
}


class Cell {
public:
    Cell(){
        owner = NEUTRAL;
    }

    bool isAmong(int p1, int p2) {
        return owner == p1 || owner == p2;
    }

    int owner;
};

#define A_IS_B_OR_C(a,b,c) ((a)==(b)||(a)==(c))

class Position{
public:
    Position() {
        x = -1;
        y = -1;
    }

    Position(int x_, int y_) {
        x = x_;
        y = y_;
    }

    int x;
    int y;

    int distance(Position other) {
        return abs(other.x-x) + abs(other.y-y);
    }

    /** moves to the Position to the given direction (if possible)
      * return true if the Position stays on the board */
    bool move(const Direction &dir) {
        bool res = true;
        switch(dir) {
        case Up:
        case UpLeft:
        case UpRight:
            if (y>0 ) {
                y--;
            } else {
                res = false;
            }
            break;
        case Down:
        case DownLeft:
        case DownRight:
            if (y<19) {
                y++;
            } else {
                res = false;
            }
            break;
        default:
            break;
        }

        switch(dir) {
        case Left:
        case UpLeft:
        case DownLeft:
            if (x>0 ){
                x--;
            } else {
                res = false;
            }
            break;
        case Right:
        case UpRight:
        case DownRight:
            if (x<34) {
                x++;
            } else {
                res = false;
            }
            break;
        default:
            break;
        }
        return res;
    }
    void secure() {
        if (y<0 ) {
            y = 0;
        }
        if (y>19) {
            y = 19;
        }
        if (x<0 ){
            x = 0;
        }
        if (x>34) {
            x = 34;
        }
    }

    bool isValid() {
        return y>=0 && y<20 && x>=0 && x<35;
    }

    void print(){
        fprintf(stderr,"(%d,%d)",x,y);
    }

    bool operator==(const Position &other) const{
        return x == other.x && y == other.y;
    }
    bool operator!=(const Position &other) const{
        return x != other.x || y != other.y;
    }

    const Position operator+(const Direction &dir) {
        Position pos = *this;
        pos.move(dir);
        return pos;
    }

    /* useful to get a vector */
    const Position operator-(const Position &other) {
        Position pos(x-other.x,y-other.y);
        return pos;
    }

    /* only makes sense on a vector */
    Direction direction() const {
        if (x == 0) {
            if (y==0) {
                return None;
            } else if (y>0) {
                return Down;
            } else {
                return Up;
            }
        } else if (x>0) {
            if (y==0) {
                return Right;
            } else if (y>0) {
                return DownRight;
            } else {
                return UpRight;
            }
        } else {
            if (y==0) {
                return Left;
            } else if (y>0) {
                return DownLeft;
            } else {
                return UpLeft;
            }
        }
    }

    void stepTowards(Position target) {
        if (x == target.x) {
            //same column
            if (y < target.y) {
                y++;
            } else if (y > target.y) {
                y--;
            }
        } else if (x < target.x) {
            x++;
        } else {
            x--;
        }
        secure();
    }

    static Position topLeft(Position &a,Position&b){
        return Position(min(a.x,b.x),min(a.y,b.y));
    }
    static Position bottomRight(Position &a,Position&b){
        return Position(max(a.x,b.x),max(a.y,b.y));
    }
};

class Player {
public:
    Position pos;
    int backInTimeLeft;
    Direction direction;
    Direction lastTurn; //Left or Right
    int score;
};

class Grid {
public:
    Cell cell[35][20]; /* 700 */
    Player player[4];

    Grid * moveAwayPattern;
    int moveAwayStep;

    Grid * previousGrid;

    Grid(){}

    Cell &cellAt(const Position pos) {
        return cell[pos.x][pos.y];
    }

    int score(int p) {
        return player[p].score;
    }

    void computeScore(int p) {
        FS
        int score = 0;
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                if (cell[x][y].owner==p) {
                    score++;
                }
            }
        }
        player[p].score = score;
        FE(4)
    }

    /* stops as soon as possible */
    bool attemptFillQuick(Position pos, int &p) {
        bool res = true;
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = TREATED;
            for (Direction dir = Up; dir <= UpLeft; ++dir) {
                Position newPos = pos;
                if (newPos.move(dir)) {
                    if (!attemptFillQuick(newPos,p)) {
                        res = false;
                        break;
                    }
                } else {
                    res = false;
                    break;
                }
            }
        } else {
            if (cellAt(pos).owner == TREATED) {
            } else if (p == NEUTRAL) {
                /* No player's cell encountered yet */
                p = cellAt(pos).owner;
            } else {
                res = p == cellAt(pos).owner;
            }
        }
        return res;
    }

    bool followBorder(Position pos_, int p) {
        //simple 4-way follow border
        bool res = true;
        Position pos = pos_;
        Direction initialMoveDir;
        Direction moveDir;
        //find border
        for (Direction dir = Up; dir <= Left; ++dir) {
            if (cellAround(pos,dir).owner == p) {
                initialMoveDir = (Direction)((dir+1)%4);
            }
        }
        moveDir = initialMoveDir;
        do {
            Direction lookDir = (Direction)((moveDir+3)%4);//look left
            int owner = fictiveOwnerAround(pos,lookDir);
            if (owner == NEUTRAL) {//go left
                moveDir = lookDir;
                pos.move(moveDir);
            } else if (owner == p) {
                moveDir = (Direction)((moveDir+1)%4);//turnRight
            } else {
                res = false;
                break;
            }
        } while (!(pos == pos_ && moveDir == initialMoveDir));
        return res;
    }

    void fill(Position pos, int p) {
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = p;
            player[p].score++;
            for (Direction dir = Up; dir <= UpLeft; ++dir) {
                Position newPos = pos;
                if (newPos.move(dir)) {
                    fill(newPos,p);
                }
            }
        }
    }

    Cell &cellAround(Position &pos_, Direction &dir){
        Position pos = pos_;
        pos.move(dir);
        return cellAt(pos);
    }
    int fictiveOwnerAround(Position &pos_, Direction &dir){
        Position pos = pos_;
        if (pos.move(dir)) {
            return cellAt(pos).owner;
        } else {
            return 9;
        }
    }

    void tryFill(Position pos, int p) {
        FS;
        Grid tempGrid = *this;
        bool canFill = tempGrid.attemptFillQuick(pos,p);
        countAttemp++;
        FE(1);
        if (canFill) {
            FS;
            fill(pos,p);
            countFill++;
            FE(2);
        }
    }

    Position closestNeutral(Position pos) {
        FS;
        Position res;
        int dist = 1;
        //           434
        //          43234
        //         4321234
        //        432101234
        //         4321234
        //          43234
        //           434
        int maxDist = max(max(pos.distance(Position(0,0)),pos.distance(Position(0,34))),
                          max(pos.distance(Position(19,0)),pos.distance(Position(19,34))) );
        /*cerr << maxDist << endl;
        pos = Position(10,10);
        pos.print(); cerr << endl;
        int output[20][35];
        memset(output,0,700*sizeof(int));
        output[pos.y][pos.x] = 7;
        int count = 0;*/
        while (dist <= maxDist) {
            int x;
            int y;
            //top left
            x = max(0,pos.x-dist);
            y = pos.y - (dist-pos.x+x);
            while (y>=0 && x<=pos.x) {
                /*count++;
                output[y][x] = dist;*/
                if (cell[x][y].owner == NEUTRAL) {
                    res = Position(x,y);
                    goto returnClosestNeutral;
                }
                x++;
                y--;
            }
            //bottom left
            x = max(0,pos.x-dist+1);
            y = pos.y + (dist-pos.x+x);
            while (y<20 && x<=pos.x) {
                /*count++;
                output[y][x] = dist;*/
                if (cell[x][y].owner == NEUTRAL) {
                    res = Position(x,y);
                    goto returnClosestNeutral;
                }
                x++;
                y++;
            }
            //top right
            x = min(34,pos.x+dist);
            y = pos.y - (pos.x+dist-x);
            while (y>=0 && x>pos.x) {
                /*count++;
                output[y][x] = dist;*/
                if (cell[x][y].owner == NEUTRAL) {
                    res = Position(x,y);
                    goto returnClosestNeutral;
                }
                x--;
                y--;
            }
            //bottom right
            x = min(34,pos.x+dist-1);
            y = pos.y + (pos.x+dist-x);
            while (y<20 && x>pos.x) {
                /*count++;
                output[y][x] = dist;*/
                if (cell[x][y].owner == NEUTRAL) {
                    res = Position(x,y);
                    goto returnClosestNeutral;
                }
                x--;
                y++;
            }
            dist++;
        }
        returnClosestNeutral:
/*
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                cerr << output[y][x]%10;
            }
            cerr  << endl;
        }
        cerr << count << endl;
*/
        FE(3);
        return res;
    }

    void moveMe(Direction dir) {
        if (strategy == AssumeNoAgression) {
            for (int p = 1; p< nbPlayers; p++) {
                autoMovePlayer(p);
            }
        }
        movePlayer(0,dir);
    }

    void autoMovePlayer(int p) {
        if (player[p].direction != None) {
            Direction newDir[4] = {player[p].direction, // Straight
                                   (Direction)((player[p].direction+(player[p].lastTurn==Left?3:1))%4), //Last Turn
                                   (Direction)((player[p].direction+(player[p].lastTurn==Left?1:3))%4), //opposite of last turn
                                   (Direction)((player[p].direction+2)%4)}; // reverse
            int d;
            for (d = 0; d < 4; d++) {
                if (cellAround(player[p].pos,newDir[d]).owner == NEUTRAL) {
                    movePlayer(p,newDir[d]);
                    player[p].direction = newDir[d];
                    if (d == 2) {
                        player[p].lastTurn = player[p].lastTurn==Left?Right:Left;
                    }
                    break;
                }
            }
            if (d >= 4) {
                movePlayer(p,closestNeutral(player[p].pos));
            }
        }
    }

    void movePlayer(int p, Direction dir){
        player[p].pos.move(dir);
        CheckAndFill(p);
    }

    void movePlayer(int p, Position target){
        player[p].pos.stepTowards(target);
        CheckAndFill(p);
    }

    void CheckAndFill(int p) {
        countMove++;
        if (countMove%1500 == 0) {
            checkTimeOut();
        }
        Position pos = player[p].pos;
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = p;
            player[p].score++;
            for (int i=1; i<8; i = i+2) {
                if (fictiveOwnerAround(pos,around[i]) == p &&
                    fictiveOwnerAround(pos,around[(i+2)%8]) == p &&
                    fictiveOwnerAround(pos,around[(i+1)%8]) != p) {
                    /* .o.
                     * .Oo corner
                     * ...        */
                    if (fictiveOwnerAround(pos,around[(i+1)%8]) == NEUTRAL) {
                        //cerr << "cs" << i ; print9Cells(pos,p);
                        tryFill(pos + around[(i+1)%8],p);
                    }
                    if (fictiveOwnerAround(pos,around[(i+1)%8]) != p &&
                        A_IS_B_OR_C(fictiveOwnerAround(pos,around[(i+3)%8]),NEUTRAL,p) &&
                        fictiveOwnerAround(pos,around[(i+4)%8]) == NEUTRAL &&
                        A_IS_B_OR_C(fictiveOwnerAround(pos,around[(i+5)%8]),NEUTRAL,p) &&
                        fictiveOwnerAround(pos,around[(i+6)%8]) == NEUTRAL &&
                        A_IS_B_OR_C(fictiveOwnerAround(pos,around[(i+7)%8]),NEUTRAL,p) ) {
                        //cerr << "cb" << i ; print9Cells(pos,p);
                        tryFill(pos + around[(i+4)%8],p);
                    }
                }
            }
            for (int i=1; i<8; i = i+2) {
                if (fictiveOwnerAround(pos,around[i]) == p &&
                    fictiveOwnerAround(pos,around[(i+4)%8]) == p) {
                    /* .o.
                     * .O. line
                     * .o.        */
                    if (A_IS_B_OR_C(fictiveOwnerAround(pos,around[(i+1)%8]),NEUTRAL,p) &&
                        fictiveOwnerAround(pos,around[(i+2)%8]) == NEUTRAL   &&
                        A_IS_B_OR_C(fictiveOwnerAround(pos,around[(i+3)%8]),NEUTRAL,p)    ) {
                        if (!(fictiveOwnerAround(pos,around[(i+5)%8]) == p &&
                              fictiveOwnerAround(pos,around[(i+6)%8]) == p &&
                              fictiveOwnerAround(pos,around[(i+7)%8]) == p   ) ) {
                            //cerr << "l" << i ; print9Cells(pos,p);
                            tryFill(pos + around[(i+2)%8],p);
                        }
                    }
                }
            }
        }
    }

    std::vector<Grid *> nextSteps() {
        std::vector<Grid *> steps;
        bool isDeadEnd = true;
        countExpansions++;
        for (Direction dir = Up; dir <= Left; ++dir) {
            Grid *nextGrid = NULL;
            Position pos = player[0].pos;
            if (pos.move(dir)) {
                if (cellAt(pos).owner == NEUTRAL) {
                    nextGrid = gridFactory.getNewGrid();
                    *nextGrid = *this;
                    nextGrid->previousGrid = this;
                    nextGrid->moveMe(dir);
                    //Acceleration
                    for (int i = 1; i<stepSize; i++) {
                        if (nextGrid->cellAround(nextGrid->player[0].pos,dir).owner == NEUTRAL) {
                            nextGrid->moveMe(dir);
                        } else {
                            break;
                        }
                    }
                    steps.push_back(nextGrid);
                    isDeadEnd = false;
                }
            }
        }
        if (isDeadEnd) {
            //There is no Neutral cell around, go away from here
            Position pos;
            if (moveAwayPattern == NULL) {
                pos = player[0].pos;
                //cerr << "New " << endl;
                moveAwayPattern = gridFactory.getNewGrid();
                for (int y = 0; y < 20; y++) {
                    for (int x = 0; x < 35; x++) {
                        moveAwayPattern->cell[x][y].owner = pos.distance(Position(x,y));
                    }
                }
                moveAwayPattern->cellAt(pos).owner = TREATED;
                moveAwayStep = 0;
            }
            moveAwayStep++;
            for (Direction dir = Up; dir <= Left; ++dir) {
                pos = player[0].pos;
                if (pos.move(dir)) {
                    if (moveAwayPattern->cellAt(pos).owner == moveAwayStep) {
                        moveAwayPattern->cellAt(pos).owner = TREATED;
                        Grid *nextGrid = gridFactory.getNewGrid();
                        *nextGrid = *this;
                        nextGrid->previousGrid = this;
                        nextGrid->moveMe(dir);
                        steps.push_back(nextGrid);
                    }
                }
            }
        } else {
            moveAwayPattern = NULL;
        }
        return steps;
    }

    Grid(char *trace) {
        int linesRead = 0;
        int charIdx = 0;
        while (linesRead < 20) {
            char currentChar=trace[charIdx];
            if (currentChar == '|' ||
                currentChar == '>' ) {
                charIdx++;
                for (int x = 0; x < 35; x++ ) {
                    currentChar=trace[charIdx];
                    switch (currentChar) {
                    case '.':
                    case ' ':
                    case '+':
                    case '-':
                    case '=':
                    case '$':
                    case '?':
                        cell[x][linesRead].owner = NEUTRAL;
                        break;
                    case 'O':
                    case '4':
                        player[0].pos = Position(x,linesRead);
                    case 'o':
                    case '0':
                        cell[x][linesRead].owner = 0;
                        break;
                    case 'X':
                    case '5':
                        player[1].pos = Position(x,linesRead);
                    case 'x':
                    case '1':
                        cell[x][linesRead].owner = 1;
                        break;
                    case 'V':
                    case '6':
                        player[2].pos = Position(x,linesRead);
                    case 'v':
                    case '2':
                        cell[x][linesRead].owner = 2;
                        break;
                    case 'I':
                    case '7':
                        player[3].pos = Position(x,linesRead);
                    case 'i':
                    case '3':
                        cell[x][linesRead].owner = 3;
                        break;
                    }
                    charIdx++;
                }
                charIdx++;
                linesRead++;
            }
            charIdx++;
        }

    }

    void print(Grid* pathEnd = NULL) {
        // chars with fixed width in last battles view: "#$+0123456789<=>E"
        cerr << ">>>00000000001111111111222222222233333<<<" << endl;
        cerr << ">>>01234567890123456789012345678901234<<<" << endl;
        cerr << ">>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<" << endl;
        char output[20][35];
        char ownerChars[5]  = "0123";
        char playerChars[5] = "4567";
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                int owner = cell[x][y].owner;
                char c = owner==NEUTRAL?'=':'$';
                if (owner>=0) {
                    c = ownerChars[owner];
                }
                output[y][x] = c;
            }
        }
        while (pathEnd != NULL) {
            Position pos = pathEnd->player[0].pos;
            output[pos.y][pos.x] = '+';
            pathEnd = pathEnd->previousGrid;
        }
        for (int p=0; p<nbPlayers; p++) {
            if (player[p].pos.isValid()) {
                output[player[p].pos.y][player[p].pos.x] = playerChars[p];
            }
        }
        for (int y = 0; y < 20; y++) {
            fprintf(stderr,"%02d>",y);
            for (int x = 0; x < 35; x++) {
                cerr << output[y][x];
            }
            fprintf(stderr,"<%d\n",y);
        }
        cerr << ">>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<" << endl;
        cerr << ">>>00000000001111111111222222222233333<<<" << endl;
        cerr << ">>>01234567890123456789012345678901234<<<" << endl;
    }

    void printPath(int p) {
        char output[20][35];
        Position pos;
        Position topLeft(34,19);
        Position bottomRight(0,0);
        memset(output,' ',20*35*sizeof(char));
        Grid *grid = this;
        while (grid != NULL) {
            pos = grid->player[p].pos;
            output[pos.y][pos.x] = '+';
            topLeft = Position::topLeft(topLeft,pos);
            bottomRight = Position::bottomRight(bottomRight,pos);
            grid = grid->previousGrid;
        }
        output[pos.y][pos.x] = 'O';

        for (int y = topLeft.y; y <= bottomRight.y; y++) {
            for (int x = topLeft.x; x <= bottomRight.x; x++) {
                cerr << output[y][x];
            }
            cerr << endl;
        }
    }

    void print9Cells(Position pos, int p) {
        cerr << "-" << endl;
        for (int y = max(0,pos.y-1); y<=min(19,pos.y+1); y++) {
            for (int x = max(0,pos.x-1); x<=min(34,pos.x+1); x++){
                if (pos.x==x && pos.y==y) {
                    cerr << "X";
                } else {
                    cerr << (cell[x][y].owner==NEUTRAL?"=":
                             cell[x][y].owner==p?"0":"7");
                }
            }
            cerr << endl;
        }
    }

    int pathLen() {
        FS
        Grid *grid = this;
        int res = 0;
        while (grid->previousGrid != NULL) {
            res+=grid->player[0].pos.distance(grid->previousGrid->player[0].pos);
            grid = grid->previousGrid;
        }
        FE(0)
        return res;
    }

    Position pathAt(int i) {
        std::deque<Position> path;
        Grid *grid = this;
        while (grid != NULL) {
            path.push_front(grid->player[0].pos);
            grid = grid->previousGrid;
        }

        for (int i = 0; i < path.size(); i++) {
            path[i].print();
        }

        return path.at(i);
    }

};

class GridCache {
public:
    Grid data[GRID_CACHE_SIZE];
    int current;

    GridCache() {
        current = 0;
    }
    bool isFull() {
        return current == GRID_CACHE_SIZE;
    }
    Grid *getNext() {
        return &data[current++];
    }
    Grid *at(int i) {
        return &data[i];
    }
};

GridFactory::GridFactory() {
    current = 0;
    cache.push_back(new GridCache());
}

Grid *GridFactory::getNewGrid() {
    if (cache.at(current)->isFull()) {
        current++;
        if (current>=cache.size()) {
            cache.push_back(new GridCache());
        } else {
            cache.at(current)->current = 0;
        }
    }
    return cache.at(current)->getNext();
}
void GridFactory::reset() {
    current = 0;
    cache.at(0)->current = 0;
}

int GridFactory::capacity(){
    return cache.size()*GRID_CACHE_SIZE;
}

int GridFactory::size(){
    return cache.at(current)->current + current*GRID_CACHE_SIZE;
}

class TimeLine {
public:
    Grid * currentGrid() {
        return &(grid[round]);
    }

    Grid grid[350];
    int round; /* starts at 0, contrarily to input */
};

class TimeLines {
public:
    TimeLine mainLine;
    TimeLine lostLines[4]; /* one per player */
};

TimeLines timeLines;


int currentScore;

int bestScore;
Grid *bestGrid;
float bestScorePerStep;

void analyseGrids(vector<Grid*> &grids) {
    for (int i = 0; i<grids.size(); i++) {
        Grid *nextGrid = grids[i];
        int nextScore = nextGrid->score(0);
        if (nextScore > bestScore) {
            int nextPathLen = nextGrid->pathLen();

            float selectionCriteria;
            //selectionCriteria = ((float)(nextScore-currentScore))/((float)(nextPathLen)); // immediate gain
            //selectionCriteria = ((float)nextScore)/((float)(nextPathLen+gameRound)); //long-term gain
            selectionCriteria = ((float)(nextScore-currentScore+20))/((float)(nextPathLen+20));
            if (selectionCriteria > bestScorePerStep) {
                bestScore = nextScore;
                bestGrid = nextGrid;
                bestScorePerStep = selectionCriteria;
                /*fprintf(stderr,"Best=%d:%d=%.2f \n",
                        bestScore-currentScore,bestGrid->pathLen(),bestScorePerStep);
                bestGrid->printPath(0);*/
            }
        }
    }
}

int main()
{
    int opponentCount; // Opponent count
    cin >> opponentCount; cin.ignore();
    nbPlayers = opponentCount+1;

    cerr << "nbPlayers=" << nbPlayers << endl;
    timeLines.mainLine.round = -1;

    // game loop
    while (1) {
        int gameRound;
        TimeLine *currentTimeLine = &(timeLines.mainLine);
        Grid *currentGrid;

        timeout = false;

        countFollow = 0;
        countAttemp = 0;
        countFill = 0;
        countMove = 0;

        cin >> gameRound; cin.ignore();
        gameRound--;
        cerr << "Round: " << gameRound << endl;

        /* detect back in time actions here, and backup current timeline */
        if (currentTimeLine->round != gameRound-1) {
            int nbLostRounds = currentTimeLine->round - (gameRound-1);
            cerr << "BACK IN TIME " << nbLostRounds << " ROUNDS" << endl;
        }

        currentTimeLine->round = gameRound;
        currentGrid = currentTimeLine->currentGrid();

        for (int i = 0; i < nbPlayers; i++) {
            int x; // x position
            int y; // y position
            int backInTimeLeft; // Remaining back in time
            cin >> x >> y >> backInTimeLeft; cin.ignore();
            currentGrid->player[i].pos = Position(x,y);
            currentGrid->player[i].backInTimeLeft = backInTimeLeft;
        }
        for (int i = 0; i < 20; i++) {
            string line; // One line of the map ('.' = free, '0' = you, otherwise the id of the opponent)
            cin >> line; cin.ignore();
            for (int rowIdx = 0; rowIdx < 35; rowIdx++) {
                switch (line[rowIdx]) {
                case '.':
                    currentGrid->cell[rowIdx][i].owner = NEUTRAL;
                    break;
                default:
                    currentGrid->cell[rowIdx][i].owner = line[rowIdx] - '0';
                    break;
                }
            }
        }
        roundStart = high_resolution_clock::now();

        for (int p = 0; p < nbPlayers; p++) {
            currentGrid->computeScore(p);
        }

        if (gameRound == 0) {
            /* assume players are going to go towards the closest corner, horizontally then vertically */
            for (int p = 0; p < nbPlayers; p++) {
                Position pos = currentGrid->player[p].pos;
                if (pos.x < 18) {
                    if (pos.y < 10) {
                        currentGrid->player[p].direction = Left;
                        currentGrid->player[p].lastTurn = Right;
                    } else {
                        currentGrid->player[p].direction = Left;
                        currentGrid->player[p].lastTurn = Left;
                    }
                } else {
                    if (pos.y < 10) {
                        currentGrid->player[p].direction = Right;
                        currentGrid->player[p].lastTurn = Left;
                    } else {
                        currentGrid->player[p].direction = Right;
                        currentGrid->player[p].lastTurn = Right;
                    }
                }
            }
        }

        if (gameRound>0) {
            for (int p = 0; p < nbPlayers; p++) {
                Player previousPlayer = currentTimeLine->grid[currentTimeLine->round-1].player[p];
                Player *currentPlayer = &(currentGrid->player[p]);
                if (!currentPlayer->pos.isValid()) {
                    currentPlayer->direction = None;
                } else {
                    currentPlayer->direction = (currentPlayer->pos - previousPlayer.pos).direction();
                    currentPlayer->lastTurn  = previousPlayer.lastTurn;
                    if ((previousPlayer.direction+1)%4 == currentPlayer->direction) {
                        currentPlayer->lastTurn = Right;
                    } else if ((previousPlayer.direction-1+4)%4 == currentPlayer->direction ) {
                        currentPlayer->lastTurn = Left;
                    }
                }
                //fprintf(stderr,"Player %d:\n",p);
                //print(currentPlayer->direction);
                //printTurn(currentPlayer->lastTurn);
            }
        }



        /*
        {
            // Unit-test for fillSurroundedAreas
            Grid testGrid = *currentGrid;
            testGrid.cell[4][4].owner = 0;
            testGrid.cell[4][5].owner = 0;
            testGrid.cell[4][6].owner = 0;
            testGrid.cell[4][7].owner = 0;
            testGrid.cell[4][8].owner = 0;
            testGrid.cell[6][4].owner = 0;
            testGrid.cell[6][5].owner = 0;
            testGrid.cell[6][6].owner = 0;
            testGrid.cell[6][7].owner = 0;
            testGrid.cell[6][8].owner = 0;
            testGrid.cell[5][4].owner = 0;
            testGrid.cell[5][8].owner = 0;
            testGrid.fillSurroundedAreas();
            testGrid.print();
        }*/


        /*
        //Unit-test for findBestPath
        char *trace = {" 0|                       iiiiiiiiiiii|0\n"\
                       " 1|                         oooiiiiiii|1\n"\
                       " 2|                         o oiiiiiii|2\n"\
                       " 3|       vvvvvvvvvvvvvvvvvvo oiiiiiii|3\n"\
                       " 4|xxxxxxxxxxxxxxxx        vo Oiiiiiii|4\n"\
                       " 5|x          x            vo..iiiiiii|5\n"\
                       " 6|xvvvvvvvvvvx            vo..iiiiiii|6\n"\
                       " 7|xvvvvvvvvvvxiiiiiiiiiiiiviiiioooooo|7\n"\
                       " 8|xvvvvvvvvvvxi           voooooooooo|8\n"\
                       " 9|xvvvvvvvvvvxi          ivoooooooooo|9\n"\
                       "10|xvV xxxxxxxxix   iiiiiiivoooooooooo|10\n"\
                       "11|xv        xxix   i     ivoooooooooo|11\n"\
                       "12|xv        xxix   I     ivoooooooooo|12\n"\
                       "13|xv          ix         ivoooooooooo|13\n"\
                       "14|xvvvvvvv    ix         ivoooooooooo|14\n"\
                       "15|x      v    ix         ivoooooooooo|15\n"\
                       "16|x      v    ixxxX      ivoooooooooo|16\n"\
                       "17|x      v    iiiiiiiiiiiivoooooooooo|17\n"\
                       "18|x      vvvvvvvvvvvvvvvvvvoooooooooo|18\n"\
                       "19|xxxxxxxxxxxx             oooooooooo|19"};
        Grid testGrid(trace);
        //testGrid.print();
        currentGrid = &testGrid;
        */

/*
        //test 2 for dead ends
        char *trace = {" 0|xXooooooooooooooooooooooooooooooooo|0\n"\
                       " 1|x oooooOooooooooooooooooooooooooooo|1\n"\
                       " 2|x ooooooooooooooooooooooooooooooooo|2\n"\
                       " 3|x  oooooooooooooooooooooooooooooooo|3\n"\
                       " 4|xxxxxxxxxxxxooooooooooooooooooooooo|4\n"\
                       " 5|xxxxxxxxxxxxooooooooooooooooooooooo|5\n"\
                       " 6|xxxxxxxxxxxxooooooooooooooooooooooo|6\n"\
                       " 7|xxxxxxxxxxxxooooooooooooooooooooooo|7\n"\
                       " 8|xxxxxxxxxxxxxxxxxxxxxxxxooooooooooo|8\n"\
                       " 9|xxxxxxxxxxxxxxxxxxxxxxxx ooooooooo |9\n"\
                       "10|xxxxxxxxxxxxxxxxxxxxxxxx ooooooo   |10\n"\
                       "11|xxxxxxxxxxxxxxxxxxxxxxxx           |11\n"\
                       "12|xxxxxxxxxxxxxxxxxxxxxxxx           |12\n"\
                       "13|xxxxxxxxxxxxxxxxxxxxxxxx           |13\n"\
                       "14|xxxxxxxxxxxxxxxxxxxxxxxx           |14\n"\
                       "15|xxxxxxxxxxxxxxxxxxxxxxxx           |15\n"\
                       "16|xxxxxxxxxxxxxxxxxxxxxxxx           |16\n"\
                       "17|xxxxxxxxxxxxxxxxxxxxxxxx           |17\n"\
                       "18|xxxxxxxxxxxxxxxxxxxxxxxx           |18\n"\
                       "19|xxxxxxxxxxxxxxxxxxxxxxxx           |19\n"\
                       "  +-----------------------------------+  \n"};
        Grid testGrid(trace);
        //testGrid.print();
        currentGrid = &testGrid;
*/

        /*
        //test 3 for dead ends
        nbPlayers = 4;
        char *trace = {"00>33333333333333333333333333333333333<0\n"\
                       "01>3==00000000007000000001111111111113<1\n"\
                       "02>3==0=======3==000000041111111111113<2\n"\
                       "03>3==0=======111111111111111111111113<3\n"\
                       "04>33333333333111111111111111111111113<4\n"\
                       "05>00000000333111111111111111111111113<5\n"\
                       "06>00000000===111111111111111111111113<6\n"\
                       "07>00000000===111111111111111111111113<7\n"\
                       "08>00000000000111111111111111111111113<8\n"\
                       "09>00000000000111111111111111111111113<9\n"\
                       "10>00000000000111111111111111111111113<10\n"\
                       "11>00000000000111111111111111111111113<11\n"\
                       "12>00000000000111111111111111111111113<12\n"\
                       "13>0000000000000000==111111111111111=3<13\n"\
                       "14>0000000000000000=1511111111111111=3<14\n"\
                       "15>0000000000000000=1111111211111111=3<15\n"\
                       "16>0000000000000000=1111111=11111111=3<16\n"\
                       "17>0000000000000000=1111111=11111111=3<17\n"\
                       "18>000000000000000001111111=11111111=3<18\n"\
                       "19>00000000000000000333333333333333333<19\n"};
        Grid testGrid(trace);
        testGrid.print();
        currentGrid = &testGrid;
        */
        /*
        //test 4
        nbPlayers = 2;
        char *trace = {"00>===================================<0\n"\
        "01>===================================<1\n"\
        "02>=========00000000==================<2\n"\
        "03>=========00000000==================<3\n"\
        "04>=======0000000000==================<4\n"\
        "05>=======0000000000==================<5\n"\
        "06>=====000000000000==================<6\n"\
        "07>=====000000000000==================<7\n"\
        "08>=====000000000000==================<8\n"\
        "09>====0000000000000==================<9\n"\
        "10>====0000000000000==11111==11111====<10\n"\
        "11>====0000000000000==1======1========<11\n"\
        "12>====0000000000000==1======1========<12\n"\
        "13>====0000000000000001======1==1111==<13\n"\
        "14>====00000000======01======1====1===<14\n"\
        "15>====00000000======01======1====5===<15\n"\
        "16>====00000000======01111111111111===<16\n"\
        "17>====00000000======0================<17\n"\
        "18>====00000000======0================<18\n"\
        "19>====00000000===4000================<19\n"};
        Grid testGrid(trace);
        //testGrid.print();
        currentGrid = &testGrid;*/

        /*
        //test 5
        nbPlayers = 4;
        char *trace = {"00>22222222222222222000000033333333333<0\n"\
                       "01>22222220000000002000000000333333333<1\n"\
                       "02>22222222200000002000000000333333333<2\n"\
                       "03>22222011200000002000000003333333333<3\n"\
                       "04>22222011200000002000000003333333333<4\n"\
                       "05>22222011200000002000000003333333333<5\n"\
                       "06>22222011200000002000000003333333333<6\n"\
                       "07>22222011200000002000000003333333333<7\n"\
                       "08>22222111111000002000000033333333333<8\n"\
                       "09>22222211111111222000000033333333333<9\n"\
                       "10>22222222222222233000000033333333333<10\n"\
                       "11>00017111111111111111113333333333333<11\n"\
                       "12>00012111111111111111113333333333333<12\n"\
                       "13>61112111111111111111113333333333333<13\n"\
                       "14>22222111111111111111113333333333333<14\n"\
                       "15>===+=111111111111111113333333333333<15\n"\
                       "16>=====111111111111111113333333333333<16\n"\
                       "17>==++=111111111111111113333333333333<17\n"\
                       "18>=====111111111111111113333333333333<18\n"\
                       "19>=====111111111133333333333333333333<19\n"};
        Grid testGrid(trace);
        testGrid.player[0].pos = Position(0,13);
        testGrid.player[1].pos = Position(0,13);
        testGrid.player[2].pos = Position(0,13);
        testGrid.cell[0][13].owner = NEUTRAL;
        //testGrid.print();
        currentGrid = &testGrid;
        */

        /*
        //test 6
        nbPlayers = 3;
        char *trace = {"00>=000000000====1111111111===========<0\n"\
                       "01>00=======00===1111111111===========<1\n"\
                       "02>0========00===1111111111===========<2\n"\
                       "03>0========0====1111111111===========<3\n"\
                       "04>0========0====11111111112222222222=<4\n"\
                       "05>0========0====11111111112222222222=<5\n"\
                       "06>0========0====11111111112222222222=<6\n"\
                       "07>0========0====11111111112222222222=<7\n"\
                       "08>0=======+==+==11111111112222222222=<8\n"\
                       "09>0=============11=======12222222222=<9\n"\
                       "10>0=============11=======12222222222=<10\n"\
                       "11>0=======+==40=1========12222222222=<11\n"\
                       "12>0===========0==5=======12222222222=<12\n"\
                       "13>0===========0=11111111116222222222=<13\n"\
                       "14>0=======+==+0222222222222==========<14\n"\
                       "15>0===========02=====================<15\n"\
                       "16>00=========002=====================<16\n"\
                       "17>=00=======00=222222================<17\n"\
                       "18>==00======0========================<18\n"\
                       "19>===00000000========================<19\n"};
        Grid testGrid(trace);
        gameRound = 74;
        testGrid.player[1].direction = Left;
        testGrid.player[1].lastTurn = Left;
        //testGrid.print();
        currentGrid = &testGrid;
        */

        currentGrid->moveAwayPattern = NULL;
        currentGrid->previousGrid = NULL;
        currentScore = currentGrid->score(0);
        bestScore = currentScore;
        bestGrid = currentGrid;
        bestScorePerStep = 0;
        gridFactory.reset();

        int stepSizes[10] = {17,13,11,7,5,3,2,1};

        for (int stepSizeIdx = 0; stepSizeIdx < 8; stepSizeIdx++) {
            stepSize = stepSizes[stepSizeIdx];
            std::deque<Grid *> states;
            Grid *workingGrid;

            countExpansions = 0;

            std::vector<Grid *> initialGrids;
            for (Direction dir = Up; dir <= Left; ++dir) {
                Grid *previousGrid = currentGrid;
                Grid *nextGrid;
                Position pos = currentGrid->player[0].pos;
                int s = 0;
                while (pos.move(dir) && s<stepSize) {
                    if (previousGrid->cellAt(pos).owner == NEUTRAL) {
                        nextGrid = gridFactory.getNewGrid();
                        *nextGrid = *previousGrid;
                        nextGrid->previousGrid = previousGrid;
                        nextGrid->moveMe(dir);
                        initialGrids.push_back(nextGrid);
                        previousGrid = nextGrid;
                    } else {
                        break;
                    }
                    s++;
                }
            }
            analyseGrids(initialGrids);
            states.insert(states.end(),initialGrids.begin(), initialGrids.end());

            while (states.size()>0) {
                workingGrid = states.front();
                states.pop_front();

                std::vector<Grid *> nextSteps = workingGrid->nextSteps();
                states.insert(states.end(),nextSteps.begin(), nextSteps.end());

                analyseGrids(nextSteps);

                if (timeout) {
                    cerr << "TIMEOUT!!" << endl;
                    break;
                }
            }
            fprintf(stderr,"LastDepth:%d  Expansions: %d  States: %d Best=%d:%d=%.2f \n",
                    workingGrid->pathLen(),countExpansions,states.size()+countExpansions,
                    bestScore-currentScore,bestGrid->pathLen(),bestScorePerStep);

            if (timeout) {
                //cerr << "TIMEOUT!!" << endl;
                break;
            }
        }

        /*
        char * fixedWidthChars = "#$+0123456789<=>E";
        for (int c = 0; c < strlen(fixedWidthChars); c++) {
            for (int i =0; i<20; i++) {
                cerr << fixedWidthChars[c];
            }
            cerr << endl;
        }
        */

        //bestGrid->printPath(1);

        currentGrid->print(bestGrid);

        fprintf(stderr,"factory:%d/%d\n",gridFactory.size(),gridFactory.capacity());
        fprintf(stderr,"fill/attemp/move:%d/%d/%d f/a=%.2f%% a/m=%.2f%%\n",
                countFill,countAttemp,countMove,
                ((float)countFill*100)/((float)countAttemp),
                ((float)countAttemp*100)/((float)countMove));


#ifdef PROFILE
        double totalSpent = 0;
        for (int i = 0; i < 5; i++) {
            totalSpent += timeSpent[i];
        }
        for (int i = 0; i < 5; i++) {
            fprintf(stderr,"%3d: %.6f (%.2f%%)\n",i,timeSpent[i],timeSpent[i]*100.0/totalSpent);
        }
        fprintf(stderr,"Tot: %.6f (%.6f per turn)\n",totalSpent,totalSpent/(double)(gameRound+1));
#endif

        Position nextPos(-1,-1);

        if (bestGrid->pathLen()>0) {
            fprintf(stderr,"Best Path Found! pathLen=%d bestScore = %d\n",bestGrid->pathLen(),bestScore);
            nextPos = bestGrid->pathAt(1);
        } else {
            //Go to the closest neutral cell
            cerr << "Search for closest neutral cell" << endl;
            nextPos = currentGrid->closestNeutral(currentGrid->player[0].pos);
        }

        nextPos.secure();

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        cout << nextPos.x << " " << nextPos.y << endl; // action: "x y" to move or "BACK rounds" to go back in time

    }
}