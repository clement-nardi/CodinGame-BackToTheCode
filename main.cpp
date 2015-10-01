#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <chrono>

#define NEUTRAL -1
#define TREATED -2

using namespace std;
using namespace std::chrono;

class Grid;

int nbPlayers;

int countPaths;
int countFill;
bool timeout;
int accelerationLevel;
high_resolution_clock::time_point roundStart;

int depth;

void checkTimeOut(){
    high_resolution_clock::time_point end = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(end - roundStart);
    //fprintf(stderr,"%.2f\n",time_span.count());
    if (time_span.count() >0.095) {
        timeout = true;
    }
    if (time_span.count() > 0.05 && accelerationLevel == 0 ||
        time_span.count() > 0.07 && accelerationLevel == 1    ) {
        accelerationLevel++;
//        fprintf(stderr,"ACCELERATE: %d Depth:%d  Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",
//                accelerationLevel,depth,countPaths,countFill,(float)countFill/(float)countPaths*(float)100);
    }
}

enum Strategy {
    AssumeNoAgression, AssumeWorst
};

Strategy strategy = AssumeNoAgression;

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

    static Position topLeft(Position &a,Position&b){
        return Position(min(a.x,b.x),min(a.y,b.y));
    }
    static Position bottomRight(Position &a,Position&b){
        return Position(max(a.x,b.x),max(a.y,b.y));
    }
};

void printPath(Position *path) {
    /*
    int idx = 0;
    while (true) {
        Position pos = path[idx];
        if (pos.x == -1) {
            fprintf(stderr,"\n");
            break;
        } else {
            fprintf(stderr,"(%d,%d)",pos.x,pos.y);
        }
        idx++;
    }*/
    char output[20][35];
    Position topLeft(34,19);
    Position bottomRight(0,0);
    memset(output,' ',20*35*sizeof(char));
    if (path != NULL) {
        int idx = 0;
        while (true) {
            Position pos = path[idx];
            if (pos.x == -1) {
                break;
            } else {
                output[pos.y][pos.x] = idx==0?'O':'+';
                topLeft = Position::topLeft(topLeft,pos);
                bottomRight = Position::bottomRight(bottomRight,pos);
            }
            idx++;
        }
    }

    for (int y = topLeft.y; y <= bottomRight.y; y++) {
        for (int x = topLeft.x; x <= bottomRight.x; x++) {
            cerr << output[y][x];
        }
        cerr << endl;
    }
}

int pathLen(Position *path) {
    if (path != NULL) {
        int idx = 0;
        while (true) {
            Position pos = path[idx];
            if (pos.x == -1) {
                return idx;
            }
            idx++;
        }
    }
    return 0;
}

class Player {
public:
    Position pos;
    int backInTimeLeft;
    Direction direction;
    Direction lastTurn; //Left or Right
};

class Grid {
public:
    Cell cell[35][20]; /* 700 */
    Player player[4];

    Grid(){}

    Cell &cellAt(const Position pos) {
        return cell[pos.x][pos.y];
    }

    bool cellHasOpponentOnIt(int x, int y) {
        for (int p = 1; p < nbPlayers; p++) {
            if (player[p].pos.x == x &&
                player[p].pos.y == y   ) {
                return true;
            }
        }
        return false;
    }

    bool cellHasAround(int x,int y,int p) {
        for (Direction dir = Up; dir <= UpLeft; ++dir) {
            if (cellAt(Position(x,y)+dir).owner == p) {
                return true;
            }
        }
        return false;
    }

    void growPlayer(int p) {
        Grid newGrid = *this;
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                if (cell[x][y].owner == p) {
                    for (Direction dir = Up; dir <= Left; ++dir) {
                        newGrid.cellAt(Position(x,y)+dir).owner = p;
                    }
                }
            }
        }
        *this = newGrid;
    }

    void growOpponents() {
        for (int p = 1; p < nbPlayers; p++) {
            growPlayer(p);
        }
    }


    int score(int p) {
        int score = 0;
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                if (cell[x][y].owner==p) {
                    score++;
                }
            }
        }
        return score;
    }

    /** returns true if this area can be filled
      * returns the new owner in *p */
    bool attemptFill(Position pos, int *p) {
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = TREATED;
            bool res = true;
            for (Direction dir = Up; dir <= UpLeft; ++dir) {
                Position newPos = pos;
                if (newPos.move(dir)) {
                    res &= attemptFill(newPos,p);
                } else {
                    res = false;
                }
            }
            return res;
        } else {
            if (cellAt(pos).owner == TREATED) {
                return true;
            } else if (*p == NEUTRAL) {
                /* No player's cell encountered yet */
                *p = cellAt(pos).owner;
                return true;
            } else {
                return *p == cellAt(pos).owner;
            }
        }
    }

    /* stops as soon as possible */
    bool attemptFillQuick(Position pos, int &p) {
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = TREATED;
            for (Direction dir = Up; dir <= UpLeft; ++dir) {
                Position newPos = pos;
                if (newPos.move(dir)) {
                    if (!attemptFillQuick(newPos,p)) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            return true;
        } else {
            if (cellAt(pos).owner == TREATED) {
                return true;
            } else if (p == NEUTRAL) {
                /* No player's cell encountered yet */
                p = cellAt(pos).owner;
                return true;
            } else {
                return p == cellAt(pos).owner;
            }
        }
    }

    /* abandonned */
    bool followBorder(Position pos, int aroundIdx, int &p, int &fill) {
        cellAt(pos).owner = fill;
        for (int angle = 6; angle <=10; angle++) { //look left, then front, then right
            Direction dir = around[(aroundIdx+angle)%8];
            int owner = cellAround(pos,dir).owner;
            if (owner == NEUTRAL) {
                return followBorder(pos + dir,(aroundIdx+angle)%8,p,fill);
            } else if (owner == p || owner == fill) {
                /* only for front and right: test corner */
                continue;
            }
        }
    }

    void fill(Position pos, int p) {
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = p;
            for (Direction dir = Up; dir <= UpLeft; ++dir) {
                Position newPos = pos;
                if (newPos.move(dir)) {
                    fill(newPos,p);
                }
            }
        }
    }

    void fillSurroundedAreas(){
        countFill++;
        Grid tempGrid = *this;
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                if (tempGrid.cell[x][y].owner == NEUTRAL) {
                    int newOwner = NEUTRAL;
                    bool onlyOneBorder;
                    //fprintf(stderr,"found neutral cell(%d,%d), attempting fill.\n",x,y);
                    onlyOneBorder = tempGrid.attemptFill(Position(x,y),&newOwner);
                    //fprintf(stderr,"onlyOneBorder=%d, newOwner=%d\n",onlyOneBorder,newOwner);
                    //tempGrid.print();
                    if (onlyOneBorder && newOwner >= 0) {
                        //cerr << "FILLING" <<endl;
                        fill(Position(x,y),newOwner);
                    }
                }
            }
        }
    }

    Cell &cellAround(Position &pos_, Direction &dir){
        Position pos = pos_;
        pos.move(dir);
        return cellAt(pos);
    }

    void tryFill(Position pos, int p) {
        countFill++;
        Grid tempGrid = *this;
        if (tempGrid.attemptFillQuick(pos,p)) {
            fill(pos,p);
        }
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
        Direction newDir[5] = {player[p].direction, // Straight
                               (Direction)((player[p].direction+(player[p].lastTurn==Left?3:1))%4), //Last Turn
                               (Direction)((player[p].direction+(player[p].lastTurn==Left?1:3))%4), //opposite of last turn
                               (Direction)((player[p].direction+2)%4),// reverse
                               player[p].direction}; // Straight (last resort)
        for (int d = 0; d < 5; d++) {
            if (cellAround(player[p].pos,newDir[d]).owner == NEUTRAL || d == 4) {
                movePlayer(p,newDir[d]);
                player[p].direction = newDir[d];
                if (d == 2) {
                    player[p].lastTurn = player[p].lastTurn==Left?Right:Left;
                }
                break;
            }
        }
    }

    void movePlayer(int p, Direction dir){
        Position pos;
        player[p].pos.move(dir);
        pos = player[p].pos;
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = p;
            for (int i=1; i<8; i = i+2) {
                if (cellAround(pos,around[i]).owner == p &&
                    cellAround(pos,around[(i+2)%8]).owner == p) {
                    /* .o.
                     * .Oo corner
                     * ...        */
                    if (cellAround(pos,around[(i+1)%8]).owner == NEUTRAL) {
                        tryFill(pos + around[(i+1)%8],p);
                    } else if (cellAround(pos,around[(i+3)%8]).isAmong(NEUTRAL,p) &&
                               cellAround(pos,around[(i+4)%8]).owner == NEUTRAL &&
                               cellAround(pos,around[(i+5)%8]).isAmong(NEUTRAL,p) &&
                               cellAround(pos,around[(i+6)%8]).owner == NEUTRAL &&
                               cellAround(pos,around[(i+7)%8]).isAmong(NEUTRAL,p) ) {
                        for (int j = 3; j<=7; j++) {
                            if (cellAround(pos,around[(i+j)%8]).owner == NEUTRAL) {
                                tryFill(pos + around[(i+j)%8],p);
                                break;
                            }
                        }
                    }
                }
            }
            for (int i=1; i<4; i = i+2) {
                if (cellAround(pos,around[i]).owner == p &&
                    cellAround(pos,around[(i+4)%8]).owner == p) {
                    /* .o.
                     * .O. line
                     * .o.        */
                    if (cellAround(pos,around[(i+1)%8]).isAmong(NEUTRAL,p) &&
                        cellAround(pos,around[(i+2)%8]).owner == NEUTRAL   &&
                        cellAround(pos,around[(i+3)%8]).isAmong(NEUTRAL,p)    ) {
                        for (int j = 1; j<=3; j++) {
                            if (cellAround(pos,around[(i+j)%8]).owner == NEUTRAL) {
                                tryFill(pos + around[(i+j)%8],p);
                                break;
                            }
                        }
                    }
                    if (cellAround(pos,around[(i+5)%8]).isAmong(NEUTRAL,p) &&
                        cellAround(pos,around[(i+6)%8]).owner == NEUTRAL   &&
                        cellAround(pos,around[(i+7)%8]).isAmong(NEUTRAL,p)    ) {
                        for (int j = 5; j<=7; j++) {
                            if (cellAround(pos,around[(i+j)%8]).owner == NEUTRAL) {
                                tryFill(pos + around[(i+j)%8],p);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    void findBestPath(int currentPathLen,
                      int maxPathLen,
                      int *maxScore,
                      Position currentPath[],
                      Position bestPath[],
                      Grid *forbiddenCells) {
        countPaths++;
        if (timeout) return;
        if (countPaths%500 == 0) {
            checkTimeOut();
        }
        //cerr << "evaluating: " << endl;
        //currentPath[currentPathLen+1] = Position();printPath(currentPath);
        //print();
        bool isDeadEnd = true;
        if (currentPathLen < maxPathLen) {
            for (Direction dir = Up; dir <= Left; ++dir) {
                Grid nextGrid = *this;
                int newPathLen = currentPathLen;
                bool hasMoved = false;
                for (int i = 0; i<accelerationLevel+1 && currentPathLen < maxPathLen; i++) {
                    Position pos = nextGrid.player[0].pos;
                    pos.move(dir);
                    if (nextGrid.cellAt(pos).owner == NEUTRAL) {
                        nextGrid.moveMe(dir);
                        currentPath[++newPathLen] = nextGrid.player[0].pos;
                        hasMoved = true;
                    } else {
                        break;
                    }
                }
                if (hasMoved) {
                    nextGrid.findBestPath(newPathLen,
                                          maxPathLen,
                                          maxScore,
                                          currentPath,
                                          bestPath,
                                          forbiddenCells);
                    isDeadEnd = false;
                }
            }
            //There is no Neutral cell around, go away from here
            if (isDeadEnd) {
                Position pos = player[0].pos;
                Grid *newForbiddenCells;
                if (forbiddenCells == NULL) {
                    //cerr << "New " << endl;
                    newForbiddenCells = new Grid();
                    for (int y = 0; y < 20; y++) {
                        for (int x = 0; x < 35; x++) {
                            newForbiddenCells->cell[x][y].owner = currentPathLen + pos.distance(Position(x,y));
                        }
                    }
                    newForbiddenCells->cellAt(pos).owner = TREATED;
                } else {
                    newForbiddenCells = forbiddenCells;
                }
                for (Direction dir = Up; dir <= Left; ++dir) {
                    pos = player[0].pos;
                    if (pos.move(dir)) {
                        if (newForbiddenCells->cellAt(pos).owner == currentPathLen+1) {
                            newForbiddenCells->cellAt(pos).owner = TREATED;
                            Grid nextGrid = *this;
                            nextGrid.moveMe(dir);
                            currentPath[currentPathLen+1] = pos;
                            nextGrid.findBestPath(currentPathLen+1,
                                                  maxPathLen,
                                                  maxScore,
                                                  currentPath,
                                                  bestPath,
                                                  newForbiddenCells);
                            isDeadEnd = false;
                        }
                    }
                }

                if (forbiddenCells == NULL) {
                    delete newForbiddenCells;
                }
            }
        }
        if (isDeadEnd) {
            int currentScore = score(0);
            currentPath[currentPathLen+1] = Position();
            //cerr << "---" << endl;
            //printPath(currentPath);
            //print();
            //cerr << "score=" << currentScore << endl;
            //cerr << "deadEnd. Score=" << currentScore << " max=" << *maxScore << endl;
            if (currentScore>*maxScore) {
                *maxScore = currentScore;
                memcpy(bestPath,currentPath,(currentPathLen+1)*sizeof(Position));
                bestPath[currentPathLen+1] = Position();
            }
        }
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

    void print(Position path[] = NULL) {
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
        if (path != NULL) {
            int idx = 0;
            while (true) {
                Position pos = path[idx];
                if (!pos.isValid()) {
                    break;
                } else {
                    output[pos.y][pos.x] = '+';
                }
                idx++;
            }
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

};

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

        countPaths = 0;
        countFill = 0;
        timeout = false;
        accelerationLevel = 0;

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
                currentPlayer->direction = (currentPlayer->pos - previousPlayer.pos).direction();
                currentPlayer->lastTurn  = previousPlayer.lastTurn;
                if ((previousPlayer.direction+1)%4 == currentPlayer->direction) {
                    currentPlayer->lastTurn = Right;
                } else if ((previousPlayer.direction-1+4)%4 == currentPlayer->direction ) {
                    currentPlayer->lastTurn = Left;
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



#define MAX_DEPTH 112
        int maxScore = currentGrid->score(0);
        Position bestPath[701];
        Grid opponentPattern[MAX_DEPTH];
        depth = 1;

        if (strategy == AssumeWorst) {
            for (int p=0; p<nbPlayers; p++) {
                Position pos = currentGrid->player[p].pos;
                if (pos.isValid() && pos != timeLines.mainLine.grid[timeLines.mainLine.round-1].player[p].pos) {
                    opponentPattern[0].cellAt(currentGrid->player[p].pos).owner = p;
                }
            }
            for (int i = 1; i < MAX_DEPTH ; i++) {
                for (int y = 0; y < 20; y++) {
                    for (int x = 0; x < 35; x++) {
                        int owner = opponentPattern[i-1].cell[x][y].owner;
                        if (owner >= 1) {
                            for (Direction dir = Up; dir <= Left; ++dir) {
                                opponentPattern[i].cellAt(Position(x,y)+dir).owner = owner;
                            }
                        }
                    }
                }
            }
        }

        bestPath[0] = currentGrid->player[0].pos;

        while (depth < MAX_DEPTH) {
            Grid tempGrid = *currentGrid;
            Position currentPath[701];
            currentPath[0] = currentGrid->player[0].pos;

            if (strategy == AssumeWorst) {
                for (int y = 0; y < 20; y++) {
                    for (int x = 0; x < 35; x++) {
                        int owner = opponentPattern[depth].cell[x][y].owner;
                        if (owner>0) {
                            if (tempGrid.cell[x][y].owner == NEUTRAL) {
                                tempGrid.cell[x][y].owner = owner;
                            }
                        }
                    }
                }
            }

            //tempGrid.print();

            int maxScoreSaved = maxScore;
            //cerr << "DEPTH:" << depth << endl;

            tempGrid.findBestPath(0,depth,
                                  &maxScore,
                                  currentPath,
                                  bestPath,
                                  NULL);

            //checkTimeOut();
            //printPath(bestPath);
            //cerr << "maxScore = " << maxScore << endl;
            //fprintf(stderr,"Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",countPaths,countFill,(float)countFill/(float)countPaths*(float)100);
            if (maxScore == maxScoreSaved) {
                //cerr << "Going further won't help" << endl;
                //break;
            }

            if (timeout) {
                cerr << "TIMEOUT!!" << endl;
                break;
            }
            depth++;
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


        currentGrid->print(&bestPath[1]);

        fprintf(stderr,"Depth:%d  Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",
                depth,countPaths,countFill,(float)countFill/(float)countPaths*(float)100);

        Position nextPos(-1,-1);

        if (bestPath[1].x != -1) {
            fprintf(stderr,"Best Path Found! pathLen=%d maxScore = %d\n",pathLen(bestPath),maxScore);
            nextPos = bestPath[1];
        } else {
            //Go to the closest neutral cell
            cerr << "Search for closest neutral cell" << endl;
            Grid myPattern;
            myPattern.cell[currentGrid->player[0].pos.x][currentGrid->player[0].pos.y].owner = 0;

            // Grow my pattern
            bool goOn = true;
            while (goOn) {
                goOn = false;
                Grid newPattern = myPattern;
                for (int y = 0; y < 20 && nextPos.x == -1; y++) {
                    for (int x = 0; x < 35 && nextPos.x == -1; x++) {
                        if (myPattern.cell[x][y].owner==0) {
                            if (currentGrid->cell[x][y].owner==NEUTRAL &&
                                !currentGrid->cellHasOpponentOnIt(x,y)) {
                                nextPos = Position(x,y);
                                cerr << " --> found" << endl;
                            } else {
                                for (Direction dir = Up; dir <= Left; ++dir) {
                                    Position pos(x,y);
                                    pos.move(dir);
                                    newPattern.cellAt(pos).owner = 0;
                                }
                            }
                        } else {
                            goOn = true;
                        }
                    }
                }
                myPattern = newPattern;
            }
        }

        nextPos.secure();

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        cout << nextPos.x << " " << nextPos.y << endl; // action: "x y" to move or "BACK rounds" to go back in time

    }
}
