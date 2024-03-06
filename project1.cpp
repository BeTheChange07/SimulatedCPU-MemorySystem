#include <iostream>
#include <unistd.h>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <string>
#include <sstream>
#include <signal.h>

using namespace std;


//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
// FILE READER CLASS
// *********************************************************************************
//**********************************************************************************
//**********************************************************************************
class FileReader{

public:
    FileReader() {}

    int readFile(int *memArr, string fileName) {
        ifstream inputStream(fileName.c_str());


        if (!inputStream.is_open()) {
            cout << "Error when trying to open file\n";
            exit(0);
        }


        int instructionNum;
        int i = 0;
        int numberOfInstuctions = 0;
        string line;
        char firstChar;



        while (getline(inputStream, line)) {
            numberOfInstuctions++;
            stringstream ss(line);
            firstChar = line[0];


            if(firstChar == '.'){
                char removePeriod;
                ss >> removePeriod;
                ss >> instructionNum;
                i = instructionNum;

            }
            else if(ss >> instructionNum){
                memArr[i++] = instructionNum;
            }
            else{
                ss >> line;
                continue;
            }

        }
        return numberOfInstuctions; // return number of instructions to memory
    }


};




//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
// MEMORY CLASS
// *********************************************************************************
//**********************************************************************************
//**********************************************************************************
class Memory{

public:
    Memory(string fileName, int *pipeArrChildToParent, int *pipeArrParentToChild, int *pipeCheckReadWrite){
        this -> pipeArrMemToCpu = pipeArrChildToParent;
        this -> pipeArrCpuToMem = pipeArrParentToChild;
        this -> pipeCheckReadWrite = pipeCheckReadWrite;
        fillMemArr(fileName);

    }

    void fillMemArr(string fileName){
        FileReader *readFileObj = new FileReader();    // create file reader
        numOfInstruction = readFileObj->readFile(getMemArr(), fileName);
        sendNumInstrucToCpu(numOfInstruction);
        delete(readFileObj);

    }

    void sendNumInstrucToCpu(int numOfInstuc){
        checkIfWright();
        writeToCpu(numOfInstuc);


    }

    void memLoop(){
        while(true){
            if(checkIfWright()){
                writeElementToIndex();
            }
            else{
                sendElementToCpu();
            }
        }
    }

    bool checkIfWright(){
        read(pipeCheckReadWrite[0], bufferMem, sizeof(int));
        int readWriteNum = bufferMem[0];

        // write to the stack
        if(readWriteNum == 1){
            return true;
        }
        // read from next element
        else if(readWriteNum == 0){
            return false;
        }

    }


    void sendElementToCpu(){
        // getting element
        int arrayIndex = readFromCpu();
        int arrayElement = getMemArr()[arrayIndex];                               // write element at pc to the buffer
        writeToCpu(arrayElement);                                         // send element at pc to the parent

    }

    void writeElementToIndex(){
        // get sp from cpu
        int index = readFromCpu();


        // get number to put in stack
        int element = readFromCpu();
        memArr[index] = element;

    }


    int readFromCpu(){
        read(pipeArrCpuToMem[0], bufferMem, sizeof(int));
        int num = bufferMem[0];
        return num;
    }

    void writeToCpu(int num){
        bufferMem[0] = num;
        write(pipeArrMemToCpu[1], bufferMem, sizeof(int));

    }


    int* getMemArr(){
        return memArr;
    }


private:
    int memArr[2000];
    int numOfInstruction;
    int bufferMem[2];
    int *pipeArrMemToCpu ;
    int *pipeArrCpuToMem ;
    int *pipeCheckReadWrite ;

};



//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
// CPU CLASS
// *********************************************************************************
//**********************************************************************************
//**********************************************************************************
class Cpu{

public:
    Cpu(int *pipeArrChildToParent, int *pipeArrParentToChild, int *pipeCheckReadWrite, int alarmTime, int childId){

        this -> childId = childId;
        this -> pipeArrMemToCpu = pipeArrChildToParent;
        this -> pipeArrCpuToMem = pipeArrParentToChild;
        this -> pipeCheckReadWrite = pipeCheckReadWrite;
        this -> alarmTime = alarmTime;
        pc = 0;
        sp = 1000;
        readNumOfInstructionFromMem();
    }

    void readNumOfInstructionFromMem(){
        // tell memory it is a read
        tellMemToReadToCpu();


        // read the num of instruction from memory
        numOfInstruction = readFromMem();

    }

    void tellMemToReadToCpu(){
        bufferCpu[0] = 0;
        write(pipeCheckReadWrite[1], bufferCpu, sizeof(int));

    }

    int readFromMem(){
        int num;
        read(pipeArrMemToCpu[0], bufferCpu, sizeof(int));
        num = bufferCpu[0];

        return num;
    }

    void tellMemToWriteToCpu(){
        bufferCpu[0] = 1;
        write(pipeCheckReadWrite[1], bufferCpu, sizeof(int));
    }

    void writeNumToMem(int num){
        bufferCpu[0] = num;
        write(pipeArrCpuToMem[1], bufferCpu, sizeof(int) );

    }


    void cpuLoop(){
        while(true){
            if(alarmActivatedBool && !inSystemCall && !systemMode && !inAlarm){
                alarmActivated();
            }
            else{
                setIrAndExecute(getPc()); // put element at pc into ir and execute
                countOfInstruction++;
                if(countOfInstruction % alarmTime == 0 && countOfInstruction > 0 ){
                    alarmActivatedBool = true;

                }
            }
        }
    }

    void alarmActivated(){
        preformInterrupt(1000);
    }

    void preformInterrupt(int InterruptIndex){

        // save current pc and sp
        int currentUserPc = getPc();

        int currentUserSp = getSp();

        setSp(2000);
        setPc(InterruptIndex);
        systemMode = true;

        // save the current pc to system stack
        push(currentUserPc);


        // save the current stack pointer to system stack
        push(currentUserSp);

        if(InterruptIndex == 1000){
            inAlarm = true;
            alarmActivatedBool = false;
        }
        if(InterruptIndex == 1500){
            inSystemCall = true;
        }

    }

    void push(int pushNum){
        tellMemToWriteToCpu();
        writeElementToIndexFromCpuToMem(--sp, pushNum);

    }

    int pop(){
        return getElementFromMem(sp++);

    }

    void setIrAndExecute(int executeElement){
        setIr(getElementFromMem(executeElement));
        // pc++
        setPc(getPc() + 1);
        exInstruction();

    }

    int getElementFromMem(int currentPc){

        // tell memory it is a read
        tellMemToReadToCpu();

        // getting next element from memeory
        writeNumToMem(currentPc);                                               // send pc to mem

        int elementAtPc = readFromMem();

        return elementAtPc;

    }

    void writeElementToIndexFromCpuToMem(int index, int element){
        writeNumToMem(index);

        writeNumToMem(element);
    }


    void exInstruction(){
        int reg1 = 0;
        int reg2 = 0;
        int reg3 = 0;
        int reg4 = 0;


        switch (getIr()) {
            case 1: // put next element into ac
                reg1 = getElementFromMem(getPc());
                // pc++
                setPc(getPc() + 1);
                ac = reg1;
                break;

            case 2: // get next element, get element at that index
                reg1 = getElementFromMem(getPc());
                // pc++
                setPc(getPc() + 1);
                if(!systemMode && reg1 > 999){
                    cout << "Memory violation: accessing system address " << reg1 << " in user mode\n" << flush;
                }
                else if(systemMode && reg1 < 1000){
                    cout << "Memory violation: accessing user address " << reg1 << " in system mode\n" << flush;
                }
                else{
                    ac = getElementFromMem(reg1);

                }

                break;

            case 3:
                // get the next element
                reg1 = getElementFromMem(getPc());

                // pc++
                setPc(getPc() + 1);

                if(!systemMode && reg1 > 999){
                    cout << "Memory violation: accessing system address " << reg1 << " in user mode " << flush;
                }
                else if(systemMode && reg1 < 1000){
                     cout << "Memory violation: accessing user address " << reg1 << " in system mode " << flush;
                }
                else{
                    // get the index from reg1
                   reg2 = getElementFromMem(reg1);

                    if(!systemMode && reg2 > 999){
                        cout << "attempt to access system memory in user mode, load not completed" << flush;

                    }
                    else if(systemMode && reg2 < 1000){
                        cout << "attempt to access user memory in kernel mode, load not completed" << flush;
                    }
                    else{
                        ac = getElementFromMem(reg2);
                    }

                }

                break;

            case 4: // (next element + x) <- get that index and put the element at that index into ac

            // get the element at current pc from memory
            reg1 = getElementFromMem(getPc());

            // pc++
            setPc(getPc() + 1);

            // next element + x = reg2
            reg2 = reg1 + getX();

            // get element at reg2
            reg3 = getElementFromMem(reg2);



            // set ac to element
            setAc(reg3);

                break;

            case 5:
                // get the element at current pc from memory
                reg1 = getElementFromMem(getPc());

                // pc++
                setPc(getPc() + 1);

                // next element + y = reg2
                reg2 = reg1 + getY();

                // get element at reg2
                reg3 = getElementFromMem(reg2);

                // set ac to element
                setAc(reg3);

                break;

            case 6: // get sp + x register
                // sp + x = reg2
                reg2 = getX() + getSp();

                // get element at reg2
                reg3 = getElementFromMem(reg2);

                // set ac to element
                setAc(reg3);

                break;

            case 7: // store values at address of ac

            // get index to store ac
            reg1 = getElementFromMem(getPc());


            // pc++
            setPc(getPc() + 1);

            // get ac for index
            reg2 = getAc();

            // write element to index
            tellMemToWriteToCpu();
            writeElementToIndexFromCpuToMem(reg1, reg2);

            break;

            case 8: // put random number in ac
                srand(time(0));
                ac = (rand() % 100) + 1;

                break;

            case 9: // print to console based on next number
                reg1 = getElementFromMem(getPc());

                // pc++
                setPc(getPc() + 1);
                if(reg1 == 1){
                    cout << ac << flush;
                }
                else if (reg1 == 2){
                    cout << (char)ac << flush;
                }


                break;

            case 10:
                ac = ac + x;
                break;
            case 11:
                ac = ac + y;
                break;

            case 12:
                ac = ac - x;

                break;

            case 13:
                ac = ac - y;

                break;

            case 14:
                x = ac;

                break;

            case 15:
                ac = x;

                break;

            case 16:
                y = ac;

                break;

            case 17:

                ac = y;
                break;

            case 18:
                sp = ac;

                break;

            case 19:
                ac = sp;

                break;

            case 20: // jump to address

            // get the index to jump to
            reg1 = getElementFromMem(getPc());


            // set pc to jump address
            setPc(reg1);

                break;

            case 21:
                if(ac == 0){
                    reg1 = getElementFromMem(getPc());
                    setPc(reg1);
                }
                else{
                    // pc++
                    setPc(getPc() + 1);
                }

                break;

            case 22:
                if(ac != 0){
                    reg1 = getElementFromMem(getPc());
                    setPc(getPc() + 1);
                    setPc(reg1);
                }
                else{
                    setPc(getPc() + 1);
                }


                break;

            case 23: // push return address onto stack and jup to address

                // get element to jump to;
                reg2 = getElementFromMem(getPc());

                // pc++
                setPc(getPc() + 1);

                // send index of sp and pc to stack
                tellMemToWriteToCpu();
                writeElementToIndexFromCpuToMem(--sp, getPc());



                // set pc to jump location
                setPc(reg2);

                break;

            case 24:

                // get return value from stack
                reg3 = getElementFromMem(sp++);

                //set pc to reg3
                setPc(reg3);

                // set reg3 to ir and execute
                setIrAndExecute(getPc());


                break;

            case 25:
                x++;

                break;

            case 26:
                x--;

                break;

            case 27:

                // send index of sp and ac to stack
                push(getAc());

                break;

            case 28:
                ac = pop();

                break;

            case 29:
                if(!inAlarm){
                    preformInterrupt(1500);
                }

                break;

            case 30:

                // get user pc and sp
                reg1 = pop(); // getting sp
                reg2 = pop(); // get pc
                setSp(reg1);
                setPc(reg2);

                inAlarm = inSystemCall = systemMode = false;

                break;


            case 50:
                kill(childId, SIGKILL);
                exit(0);
                break;

        }

    }

    int getPc(){
        return pc;
    }
    int getSp(){
        return sp;
    }
    int getIr(){
        return ir;
    }
    int getAc(){
        return ac;
    }
    int getX(){
        return x;
    }
    int getY(){
        return y;
    }


    void setPc(int pc){
        this->pc = pc;
    }
    void setSp(int sp){
        this->sp = sp;
    }
    void setIr(int ir){
        this->ir = ir;
    }
    void setAc(int ac){
        this->ac = ac;
    }
    void setX(int x){
        this->x = x;
    }
    void setY(int y){
        this->y = y;
    }
protected:
    int pc, sp, ir, ac, x, y;
    int bufferCpu[2];
    int *pipeArrMemToCpu ;
    int *pipeArrCpuToMem ;
    int *pipeCheckReadWrite ;
    int alarmTime;
    int numOfInstruction;
    int countOfInstruction;
    bool alarmActivatedBool;
    bool systemMode;
    bool inAlarm;
    bool inSystemCall;
    int childId;


};




//**********************************************************************************
// MAIN
// *********************************************************************************
int main(int argc, char *argv[]) {

    string fileName = argv[1];              // file name form terminal
    int alarmTime = atoi(argv[2]);          // alarm time from terminal

    int pipeArrChildToParent[2];
    int pipeArrParentToChild[2];
    int pipeArrReadWriteCheck[2];



    pipe(pipeArrChildToParent);
    pipe(pipeArrParentToChild);
    pipe(pipeArrReadWriteCheck);


    pid_t pId = fork();

    if(pId == -1){
        cout << "pipe failed" << endl;
        exit(0);
    }




    // within child (MEMORY)
    if(pId == 0){


        Memory *currentMem = new Memory(fileName, pipeArrChildToParent,  pipeArrParentToChild, pipeArrReadWriteCheck);                              // create memory object

        currentMem->memLoop();

        delete(currentMem);

    }




        // within parent (CPU)
    else{

        Cpu *currentCpu = new Cpu(pipeArrChildToParent, pipeArrParentToChild, pipeArrReadWriteCheck, alarmTime, pId);                                    // create cpu object

        currentCpu->cpuLoop();

        delete(currentCpu);

    }

    return 0;

}
