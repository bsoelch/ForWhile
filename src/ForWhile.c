#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#define PAGES_CAP 1024
#define PAGE_SIZE 4096
#define PAGE_MASK 0xfff
static int64_t mem[PAGE_SIZE];//XXX? fixed pages for code/stack regions
typedef struct MemPage MemPage;
struct MemPage{
  int64_t pageId;
  int64_t* data;
  MemPage* next;
};
static MemPage memPages[PAGES_CAP];

static int64_t* notNULL(int64_t* p){
  if(p==NULL){
    fputs("out-of memory\n",stderr);exit(1);
  }
  return p;
}

typedef struct{
  int64_t* data;
  int64_t size;
  int64_t cap;
}Stack;
Stack callStack;
Stack valueStack;
static void initStack(void){
  callStack=(Stack){.data=notNULL(malloc(PAGE_SIZE*sizeof(int64_t))),.size=0,.cap=PAGE_SIZE};
  valueStack=(Stack){.data=notNULL(malloc(PAGE_SIZE*sizeof(int64_t))),.size=0,.cap=PAGE_SIZE};
}
static void cleanupStack(void){
  free(callStack.data);
  free(valueStack.data);
}

#define BLOCK_TYPE_IF '['
#define BLOCK_TYPE_FOR '('
#define BLOCK_TYPE_PROC '{'

const char* blockTypeName(int64_t blockType){
  switch(blockType){
    case BLOCK_TYPE_IF:
      return "[]";
    case BLOCK_TYPE_FOR:
      return "()";
    case BLOCK_TYPE_PROC:
      return "{}";
    default:
      return "\"unknown block type\"";
  }
}

MemPage* initPage(MemPage* target,int64_t pageId){
  target->pageId=pageId;
  target->data=notNULL(calloc(PAGE_SIZE,sizeof(MemPage)));
  target->next=NULL;
  return target;
}
int64_t* getMemory(int64_t address,bool alloc){
  int64_t pageId=((uint64_t)address)/PAGE_SIZE;
  if(pageId==0)
    return &mem[address];
  MemPage* page=&memPages[pageId%PAGES_CAP];
  if(page->pageId==0){
    if(alloc)
      return &initPage(page,pageId)->data[address&PAGE_MASK];
    return NULL;
  }
  if(page->pageId==pageId)
    return &page->data[address&PAGE_MASK];
  while(page->next!=NULL){
    page=page->next;
    if(page->pageId==pageId)
      return &page->data[address&PAGE_MASK];
  }
  if(!alloc)
    return NULL;
  page->next=malloc(sizeof(MemPage));
  if(page->next==NULL){
    fputs("out-of memory\n",stderr);exit(1);
  }
  page=page->next;
  return &initPage(page,pageId)->data[address&PAGE_MASK];
}
int64_t readMemory(int64_t address){
  int64_t* cell=getMemory(address,false);
  if(cell==NULL)
    return 0;
  return *cell;
}
void writeMemory(int64_t address,int64_t value){
  int64_t* cell=getMemory(address,value!=0);
  if(cell==NULL)
    return;
  *cell=value;
}


int64_t valCount(void){
  return valueStack.size;
}
void pushValue(int64_t val){
  if(valueStack.size>=valueStack.cap){
    int64_t newCap=valueStack.cap+PAGE_SIZE;
    valueStack.data=notNULL(realloc(valueStack.data,newCap*sizeof(int64_t)));
  }
  valueStack.data[valueStack.size++]=val;
}
int64_t peekValue(void){
  if(valueStack.size<=0)
    return 0;
  return valueStack.data[valueStack.size-1];
}
int64_t popValue(void){
  if(valueStack.size<=0)
    return 0;
  return valueStack.data[--valueStack.size];
}

bool callStackEmpty(void){
  return callStack.size<=0;
}
void callStackPush(int64_t pos){
  if(callStack.size>=callStack.cap){
    int64_t newCap=callStack.cap+PAGE_SIZE;
    callStack.data=notNULL(realloc(callStack.data,newCap*sizeof(int64_t)));
  }
  callStack.data[callStack.size++]=pos;
}
int64_t callStackPeek(void){
  if(callStackEmpty()){
    fputs("call-stack underflow\n",stderr);exit(1);
  }
  return callStack.data[callStack.size-1];
}
int64_t callStackPop(void){
  if(callStackEmpty()){
    fputs("call-stack underflow\n",stderr);exit(1);
  }
  return callStack.data[--callStack.size];
}

int64_t ipow(int64_t a,int64_t e){
  int64_t res=1;
  if(e<0){
    res=0;
  }else{
    while(e!=0){
      if(e&1){
        res*=a;
      }
      a*=a;
      e>>=1;
    }
  }
  return res;
}
int64_t lshift(int64_t a,int64_t b){
  if(b==0)
    return a;
  if(b>0){
    return b>=64?0:(int64_t)((uint64_t)a<<(uint64_t)b);
  }else{
    return b<=-64?0:(int64_t)((uint64_t)a>>(uint64_t)-b);
  }
}
int64_t rshift(int64_t a,int64_t b){
  if(b==0)
    return a;
  if(b>0){
    return b>=64?0:(int64_t)((uint64_t)a>>(uint64_t)b);
  }else{
    return b<=-64?0:(int64_t)((uint64_t)a<<(uint64_t)-b);
  }
}

static int64_t maxCallDepth=3;
void runProgram(void){
  int64_t callDepth=0;
  int64_t skipCount=0;
  bool comment=false;
  bool blockComment=false;
  bool stringMode=false;
  bool numberMode=false;
  bool escapeMode=false;

  int64_t ip=-1;//instruction pointer
  char command;
  int64_t type;
  while(true){//while program is running
    command=readMemory(ip--)&0xff;
    if(command=='\0')
      return;//reached end of program
    if(blockComment){
      if(command!='\\')
        continue;
      if(command=='\\'&&readMemory(ip--)=='\\'&&readMemory(ip--)=='\\'){
        comment=false;
        blockComment=false;
      }
      continue;
    }
    if(comment){
      if(command=='\n')
        comment=false;
      continue;
    }
    if(stringMode){
      if(escapeMode){
        escapeMode=false;
        if(skipCount>0)
          continue;//string in skipped loop
        switch(command){
        case '"':
          pushValue('"');
          break;
        case '\\':
          pushValue('\\');
          break;
        case 'n':
          pushValue('\n');
          break;
        case 't':
          pushValue('\t');
          break;
        case 'r':
          pushValue('\r');
          break;//more escape sequences
        default:
          fprintf(stderr,"unsupported escape sequence: \\%c\n",command);exit(1);
        }
      }else if(command=='\\'){
        escapeMode=true;
      }else if(command=='"'){
        stringMode=false;
        if(skipCount>0)
          continue;//string in skipped loop
        int64_t tmp=valueStack.size-callStackPop();
        pushValue(tmp);
      }else{
        if(skipCount>0)
          continue;//string in skipped loop
        pushValue(command);
      }
      continue;
    }
    if(skipCount>0){
      switch(command){
        case '[':
          callStackPush(BLOCK_TYPE_IF);
          skipCount++;
          break;
        case '(':
          callStackPush(BLOCK_TYPE_FOR);
          skipCount++;
          break;
        case '{':
          callStackPush(BLOCK_TYPE_PROC);
          skipCount++;
          break;
        case ']':
          type=callStackPop();
          if(type!=BLOCK_TYPE_IF){
            fprintf(stderr,"unexpected ']' in '%s' block\n",blockTypeName(type));exit(1);
          }
          skipCount--;
          break;
        case ')':
          type=callStackPop();
          if(type!=BLOCK_TYPE_FOR){
            fprintf(stderr,"unexpected ')' in '%s' block\n",blockTypeName(type));exit(1);
          }
          skipCount--;
          break;
        case '}':
          type=callStackPeek();
          if(type==BLOCK_TYPE_PROC){// } only closes current procedure when it is not contained in sub-block
            callStackPop();
            skipCount--;
          }
          break;
        case '"':
          stringMode=true;
          break;
        case '\\':
          comment=true;
          if(readMemory(ip--)=='\\'&&readMemory(ip--)=='\\'){// \\\ -> block comment
            blockComment=true;
          }
          break;
      }
      continue;
    }
    if(command>='0'&&command<='9'){
      if(numberMode){
        int64_t v=popValue();
        pushValue(10*v+(command-'0'));
      }else{
        pushValue(command-'0');
        numberMode=true;
      }
      continue;
    }
    numberMode=false;
    switch(command){
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9'://digits have already been handled
      case ' '://ignore spaces
        break;
      //strings&comments
      case '"':
        callStackPush(valueStack.size);
        stringMode=true;
        break;
      case '\\':
        comment=true;
        if(readMemory(ip--)=='\\'&&readMemory(ip--)=='\\'){// \\\ -> block comment
          blockComment=true;
        }
        break;
      //control flow
      case '[':
        if(popValue()!=0){
          callStackPush(BLOCK_TYPE_IF);
        }else{
          skipCount=1;
          callStackPush(BLOCK_TYPE_IF);
        }
        break;
      case ']':
        type=callStackPop();
        if(type!=BLOCK_TYPE_IF){
          fprintf(stderr,"unexpected ']' in '%s' block\n",blockTypeName(type));exit(1);
        }
        break;
      case '(':{
        int64_t n=popValue();
        if(n>0){
          callStackPush(ip);
          callStackPush(n);
          callStackPush(BLOCK_TYPE_FOR);
          pushValue(n);
        }else{
          skipCount=1;
          callStackPush(BLOCK_TYPE_FOR);
        }
        }break;
      case ')':{
        type=callStackPop();
        if(type!=BLOCK_TYPE_FOR){
          fprintf(stderr,"unexpected ')' in '%s' block\n",blockTypeName(type));exit(1);
        }
        int64_t n=callStackPop();
        int64_t x=popValue();
        n--;
        if(x!=0&&n>0){
          ip=callStackPeek();
          callStackPush(n);
          callStackPush(type);
          pushValue(n);
        }else{
          callStackPop();
        }
        }break;
      case '{':
        pushValue(ip);
        skipCount=1;
        callStackPush(BLOCK_TYPE_PROC);
        break;
      case '}':
        do{
          if(callStackEmpty()){
            fputs("unexpected '}'\n",stderr);exit(1);
            break;
          }
          type=callStackPop();//pop blocks until {} block is reached
          switch(type){//pop remaining values in block
            case BLOCK_TYPE_PROC:
              break;
            case BLOCK_TYPE_IF:
              break;
            case BLOCK_TYPE_FOR:
              callStackPop();
              callStackPop();
              break;
          }
        }while(type!=BLOCK_TYPE_PROC);
        ip=callStackPop();//return
        callDepth--;
        break;
      case '?':{
        uint64_t to=popValue();
        if(callDepth<maxCallDepth){
          callStackPush(ip);
          callStackPush(BLOCK_TYPE_PROC);
          callDepth++;
          ip=to;
        }
        }break;
      //stack manipulation
      case '.':;//drop   ## a ->
        popValue();
        break;
      case ':':{//dup   ## a -> a
        int64_t a=peekValue();
        pushValue(a);
        }break;
      case '\'':{//swap ## b a -> a b
        int64_t b=popValue();
        int64_t a=popValue();
        //re-definable combinations '+ '* '& '| '^ '=  '' as modifier
        if(readMemory(ip)=='>'){//'> can be expressed as <  => can use for bit-shift
          ip--;
          pushValue(rshift(a,b));
          break;
        }else if(readMemory(ip)=='<'){//'< can be expressed as >  => can use for bit-shift
          ip--;
          pushValue(lshift(a,b));
        }//XXX? ''/ ''% -> unsigned division/modulo
        pushValue(b);
        pushValue(a);
        }break;
      case ';':{//over ## c b a -> c b a c
        int64_t a=popValue();
        int64_t c=peekValue();
        pushValue(a);
        pushValue(c);
        }break;
      case ',':{//rotate .. ## a b c -> b c a
        int64_t count=popValue();
        if(count==0)
          break;
        if(count>0){
          if(count>valCount()){//TODO handle value-stack underflow correctly
            fputs("stack underflow",stderr);exit(1);
          }
          int64_t a=valueStack.data[valueStack.size-count];// ^ 1 2 3
          for(int64_t i=count;i>1;i--){
            valueStack.data[valueStack.size-i]=valueStack.data[valueStack.size-(i-1)];
          }
          valueStack.data[valueStack.size-1]=a;
        }else{
          count=-count;
          if(count>valCount()){
            fputs("stack underflow",stderr);exit(1);
          }
          int64_t a=valueStack.data[valueStack.size-1]; // ^ 1 2 3
          for(int64_t i=1;i<count;i++){
            valueStack.data[valueStack.size-i]=valueStack.data[valueStack.size-(i+1)];
          }
          valueStack.data[valueStack.size-count]=a;
        }
        }break;
      //memory
      case '@':{
        int64_t addr=popValue();
        pushValue(readMemory(addr));
        }break;
      case '$':{
        int64_t addr=popValue();
        writeMemory(addr,popValue());
        }break;
      //arithmetic operations
      case '+':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a+b);
        }break;
      case '-':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a-b);
        }break;
      case '*':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a*b);
        }break;
      case '/':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(b==0?0:a/b);
        }break;
      case '%':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(b==0?a:a%b);
        }break;
      case '>':{
          int64_t b=popValue();
          int64_t a=popValue();
          pushValue(a>b);
          break;
        }break;
      case '<':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a<b);
        }break;
      case '=':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a==b);
        }break;
      case '&':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a&b);
        }break;
      case '|':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a|b);
        }break;
      case '^':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(a^b);
        }break;
      case '`':{
        int64_t b=popValue();
        int64_t a=popValue();
        pushValue(ipow(a,b));
        }break;
      case '!':{
        int64_t a=popValue();
        pushValue(a==0);
        }break;
      case '~':{
        int64_t a=popValue();
        if(readMemory(ip)=='~'){//~~ would have has no effect -> use combination for negation
          ip--;
          pushValue(-a);
          break;
        }
        pushValue(~a);
        }break;
      case '_':{
        pushValue(getchar());
        }break;
      case '#':{
        int64_t v=peekValue();
        putchar(v&0xff);
        }break;
      default:
        break;
    }
  }
}


#define BUFF_CAP 1024
char buffer[BUFF_CAP];
void readCode(FILE* file){
  size_t size;
  int64_t off=-1;
  do{
    size=fread(buffer,sizeof(char),BUFF_CAP,file);
    for(size_t i=0;i<size;i++){
      writeMemory(off--,buffer[i]);
    }
  }while(size>0);
}
void codeFromCString(const char* code){
  int64_t i=0;
  while(code[i]!='\0'){
    writeMemory(-1-i,code[i]);
    i++;
  }
}
int main(int numArgs,char** args) {
  bool loadFile=true;
  char *path=NULL;
  if(numArgs==2){
    loadFile=false;
    codeFromCString(args[1]);
  }else if(numArgs>2&&(strcmp(args[1],"-f")==0)){
    loadFile=true;
    path = args[2];
  }
  if(loadFile&&path==NULL){
    printf("usage: [Filename]\n or -f [Filename]");
    return EXIT_FAILURE;
  }
  if(loadFile){
    FILE *file = fopen(path, "r");
    if(file==NULL){
      printf("cannot open file \"%s\"\n",path);
      return EXIT_FAILURE;
    }
    readCode(file);
    fclose(file);//file no longer needed (whole contents are buffered)
  }

  initStack();
  runProgram();

  puts("\n------------------");
  printf("%"PRId64":",valCount());
  while(valCount()>0){
    printf("%"PRId64" ",popValue());
  }
  puts("");
  cleanupStack();
  return EXIT_SUCCESS;
}

