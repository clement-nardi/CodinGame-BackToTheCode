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

int nbPlayers;

int countPaths;
int countFill;
bool timeout;
high_resolution_clock::time_point roundStart;

void checkTimeOut(){
    high_resolution_clock::time_point end = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(end - roundStart);
    //fprintf(stderr,"%.2f\n",time_span.count());
    if (time_span.count() >0.095) {
        timeout = true;
    }
}

enum Direction {
    Up,Down,Left,Right,UpLeft,UpRight,DownLeft,DownRight
};

Direction &operator++(Direction &dir){
    dir = (Direction)(dir + 1);
    return dir;
}

class Cell {
public:
    Cell(){
        owner = NEUTRAL;
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

    /** moves to the Position to the given direction (if possible)
      * return true if the Position stays on the board */
    bool move(Direction dir) {
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
    void print(){
        fprintf(stderr,"(%d,%d)",x,y);
    }

    bool operator==(const Position &other) const{
        return x == other.x && y == other.y;
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
                output[pos.y][pos.x] = idx==0?'O':'.';
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

class Player {
public:
    Position pos;
    int backInTimeLeft;
};

class Grid {
public:
    Cell cell[35][20]; /* 700 */
    Player player[4];

    Cell &cellAt(Position pos) {
        return cell[pos.x][pos.y];
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
            for (Direction dir = Up; dir <= DownRight; ++dir) {
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
    void fill(Position pos, int p) {
        if (cellAt(pos).owner == NEUTRAL) {
            cellAt(pos).owner = p;
            for (Direction dir = Up; dir <= DownRight; ++dir) {
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

    void movePlayer(int p, Direction dir){
        player[p].pos.move(dir);
        if (cellAt(player[p].pos).owner == NEUTRAL) {
            cellAt(player[p].pos).owner = p;
            int countMine = 0;
            for (Direction dir = Up; dir <= Right; ++dir) {
                Position pos = player[0].pos;
                if (pos.move(dir)) {
                    if (cellAt(pos).owner == 0) {
                        countMine++;
                    }
                }
            }
            if (countMine >=2 ) {
                Direction around[8] = {UpLeft, Up, UpRight, Right, DownRight, Down, DownLeft, Left};
                Position previousPos = player[0].pos;
                int changeCount = 0;
                previousPos.move(Left);

                for (int i=0; i<8; i++) {
                    Position currentPos = player[0].pos;
                    currentPos.move(around[i]);
                    Cell currentCell = cellAt(currentPos);
                    Cell previousCell = cellAt(previousPos);
                    if (previousCell.owner == 0 && currentCell.owner == NEUTRAL ||
                        previousCell.owner == NEUTRAL && currentCell.owner == 0   ) {
                        changeCount++;
                    }

                    previousPos = currentPos;
                }
                if (changeCount>2) {
                    fillSurroundedAreas();
                }
            }
        }
    }

    void findBestPath(int currentPathLen,
                      int maxPathLen,
                      int *maxScore,
                      Position currentPath[],
                      Position bestPath[]) {
        countPaths++;
        if (timeout) return;
        if (countPaths%500 == 0) {
            checkTimeOut();
        }
        currentPath[currentPathLen] = player[0].pos;
        //cerr << "evaluating: ";
        //printPath(currentPath);
        //print();
        bool isDeadEnd = true;
        if (currentPathLen < maxPathLen) {
            for (Direction dir = Up; dir <= Right; ++dir) {
                Position pos = player[0].pos;
                pos.move(dir);
                if (cellAt(pos).owner == NEUTRAL) {
                    Grid nextGrid = *this;
                    nextGrid.movePlayer(0,dir);
                    nextGrid.findBestPath(currentPathLen+1,
                                          maxPathLen,
                                          maxScore,
                                          currentPath,
                                          bestPath);
                    isDeadEnd = false;
                }
            }
        }
        if (isDeadEnd) {
            int currentScore = score(0);
            currentPath[currentPathLen+1] = Position();
            //printPath(currentPath);
            //cerr << "score=" << currentScore << endl;
            //cerr << "deadEnd. Score=" << currentScore << " max=" << *maxScore << endl;
            if (currentScore>*maxScore) {
                *maxScore = currentScore;
                memcpy(bestPath,currentPath,(currentPathLen+1)*sizeof(Position));
                bestPath[currentPathLen+1] = Position();
            }
        }
    }

    void print(Position path[] = NULL) {
        cerr << "   00000000001111111111222222222233333  " << endl;
        cerr << "   01234567890123456789012345678901234  " << endl;
        cerr << "  +-----------------------------------+  " << endl;
        char output[20][35];
        char ownerChars[5]  = "oxvi";
        char playerChars[5] = "OXVI";
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 35; x++) {
                int owner = cell[x][y].owner;
                char c = owner==NEUTRAL?' ':'?';
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
                if (pos.x == -1) {
                    break;
                } else {
                    output[pos.y][pos.x] = '.';
                }
                idx++;
            }
        }
        for (int p=0; p<nbPlayers; p++) {
            output[player[p].pos.y][player[p].pos.x] = playerChars[p];
        }
        for (int y = 0; y < 20; y++) {
            fprintf(stderr,"%2d|",y);
            for (int x = 0; x < 35; x++) {
                cerr << output[y][x];
            }
            fprintf(stderr,"|%d\n",y);
        }
        cerr << "  +-----------------------------------+  " << endl;
        cerr << "   00000000001111111111222222222233333  " << endl;
        cerr << "   01234567890123456789012345678901234  " << endl;
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


/**
 * Auto-generated code below aims at helping you parse
 * the standard input according to the problem statement.
 **/
int main()
{
    int opponentCount; // Opponent count
    cin >> opponentCount; cin.ignore();
    nbPlayers = opponentCount+1;

    cerr << "nbPlayers=" << nbPlayers << endl;
    timeLines.mainLine.round = -1;

    // game loop
    while (1) {
        roundStart = high_resolution_clock::now();
        int gameRound;
        TimeLine *currentTimeLine = &(timeLines.mainLine);
        Grid *currentGrid;

        countPaths = 0;
        countFill = 0;
        timeout = false;

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


        int maxScore = 0;
        Position bestPath[701];
        Grid opponentPattern;
        int depth = 1;

        for (int p=1; p<nbPlayers; p++) {
            opponentPattern.cell[currentGrid->player[p].pos.x][currentGrid->player[p].pos.y].owner = p;
        }

        while (true) {
            Grid tempGrid = *currentGrid;
            Position currentPath[701];

            // Grow opponent pattern
            Grid newPattern = opponentPattern;
            for (int y = 0; y < 20; y++) {
                for (int x = 0; x < 35; x++) {
                    int owner = opponentPattern.cell[x][y].owner;
                    if (owner>0) {
                        for (Direction dir = Up; dir <= Right; ++dir) {
                            Position pos(x,y);
                            pos.move(dir);
                            newPattern.cellAt(pos).owner = owner;
                        }
                    }
                }
            }
            opponentPattern = newPattern;
            for (int y = 0; y < 20; y++) {
                for (int x = 0; x < 35; x++) {
                    int owner = opponentPattern.cell[x][y].owner;
                    if (owner>0) {
                        if (tempGrid.cell[x][y].owner == NEUTRAL) {
                            tempGrid.cell[x][y].owner = owner;
                        }
                    }
                }
            }

            //tempGrid.print();

            int maxScoreSaved = maxScore;

            tempGrid.findBestPath(0,depth,
                                  &maxScore,
                                  currentPath,
                                  bestPath);


            //checkTimeOut();
            //cerr << "DEPTH:" << depth << endl;
            //fprintf(stderr,"Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",countPaths,countFill,(float)countFill/(float)countPaths*(float)100);
            if (maxScore == maxScoreSaved) {
                cerr << "Going further won't help" << endl;
                break;
            }

            if (timeout) {
                cerr << "TIMEOUT!!" << endl;
                break;
            }
            depth++;
        }

        currentGrid->print(&bestPath[1]);

        fprintf(stderr,"Depth:%d  Paths: %d  Fills: %d  Fills/Paths: %.2f%%\n",
                depth,countPaths,countFill,(float)countFill/(float)countPaths*(float)100);

        Position nextPos(-1,-1);

        if (bestPath[1].x != -1) {
            cerr << "Best Path Found! maxScore = " << maxScore << endl;
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
                            if (currentGrid->cell[x][y].owner==NEUTRAL) {
                                nextPos = Position(x,y);
                                cerr << " --> found" << endl;
                            } else {
                                for (Direction dir = Up; dir <= Right; ++dir) {
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
