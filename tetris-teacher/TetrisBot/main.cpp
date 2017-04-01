//
//  main.cpp
//  TetrisBot
//
//  This code is a mess, sorry.
//

#include <iostream>
#include <unistd.h>

// |          |
// |          |
// |          |
// |          |
// |          |
// |     x    |
// |     x xx |
// |  x  x xxx|
// |_xxx_x_xxx|

template<int w, int h>
struct Bitmap {
  uint8_t bitmap[h][w]; // [y][x] indexing : 20 rows, 10 columns each
  int width, height;
  int rotations;
  
  uint8_t height_cache[w];
  int minHeight, maxHeight;
  
  uint8_t gapCount[w]; // per column
  bool rowFullFlags[h]; // per row
  
  int color = 1;
  
  Bitmap() {
    reset();
  }
  
  void reset() {
    minHeight = h;
    maxHeight = 0;
    
    width = w;
    height = h;
    rotations = 4;
    
    bzero(bitmap, sizeof(bitmap));
    bzero(height_cache, sizeof(height_cache));
    bzero(gapCount, sizeof(gapCount));
    bzero(rowFullFlags, sizeof(rowFullFlags));
  }
  
  void rotate() {
    bool oldbit[h][w];
    memcpy(oldbit, bitmap, sizeof(bitmap));
    bzero(bitmap, sizeof(bitmap));
    
    int maxXp1 = 0;
    for(int x = width-1; x >= 0 ; --x)
      for(int y = 0; y < height; ++y)
        if(oldbit[y][x]) {
          maxXp1 = x+1;
          goto afterLoops;
        }
    
  afterLoops:
    
    for(int x = 0; x < height; ++x)
      for(int y = 0; y < maxXp1; ++y)
        bitmap[y][x] = oldbit[x][maxXp1 - 1 - y];
    
    int tmp = width;
    width = height;
    height = tmp;
    
    recache();
  }
  
  void recache();
  
  void recacheMinMaxHeight() {
    minHeight = height;
    maxHeight = 0;
    
    for(int x = 0; x < width; ++x) {
      int y = height_cache[x];
      if(y > maxHeight)
        maxHeight = y;
      if(y < minHeight)
        minHeight = y;
    }
  }
};

#pragma mark - Configure

#define DO_RENDER false

#define GAP_P1   1
#define GAP_P2   0
#define GAP_P3   0
#define GAP_OVER 0

#define GAMEOVER_COST 1e+12
#define LOOKAHEAD 1

// Splitting costs makes it look nicer.
// (And probably play worse.)
// Basically splitting means that it returns
//  cost = cost(previous recursion) * COST_SPLIT_OWN + cost(last recursion) * COST_SPLIT_RECURSION
// instead of
//  cost = cost(last recursion)

#define COST_SPLIT_OWN 0.3
#define COST_SPLIT_RECURSION (1-COST_SPLIT_OWN)

// LOOKAHEAD(0) can apparently do ~10,000
// LOOKAHEAD(1) can apparently do 100,000+ (didn't bother testing further)
// LOOKAHEAD(2) ... ?
// LOOKAHEAD(3) ?!?!?! Why would you do this?!

#define MAX_TEST_ITERATIONS 100000

template<int ww, int hh>
double get_cost(Bitmap<ww, hh> &state, int completeLines) {
  double aggregateHeight = 0;
  double holes = 0;
  double bumpiness = 0;
  
  int height, prevHeight = -1;
  for(int x = 0; x < state.width; ++x) {
    holes += state.gapCount[x];
    
    height = state.height_cache[x];
    aggregateHeight += height;
    
    if(prevHeight != -1)
        bumpiness += abs(prevHeight - height);
    
    prevHeight = height;
  }
  
  // Totally not stolen from
  // https://codemyroad.wordpress.com/2013/04/14/tetris-ai-the-near-perfect-player/
  
  double a = -0.510066;
  double b = 0.760666;
  double c = -0.35663;
  double d = -0.184483;
  
  //printf("a => %lf\nb => %d\nc => %lf\nd => %lf\n", aggregateHeight, completeLines, holes, bumpiness);
  
  return -(a * aggregateHeight + b * completeLines + c * holes + d * bumpiness);
}

template<int w, int h>
void Bitmap<w, h>::recache() {
  minHeight = height;
  maxHeight = 0;
  
  for(int x = 0; x < width; ++x)
    for(int y = height; y >= 0; --y)
      if(y == 0 || bitmap[y-1][x]) {
        height_cache[x] = y;
        if(y > maxHeight)
          maxHeight = y;
        if(y < minHeight)
          minHeight = y;
        break;
      }
  
  for(int x = 0; x < width; ++x) {
    gapCount[x] = 0;
    for(int y = 1; y < height_cache[x]; ++y)
      if(!bitmap[y-1][x] && bitmap[y][x]) {
        gapCount[x] += GAP_P1;
        if(y >= 2 && !bitmap[y-2][x]) gapCount[x] += GAP_P2;
        if(y >= 3 && !bitmap[y-3][x]) gapCount[x] += GAP_P3;
        if(y+1 < height_cache[x] && bitmap[y+1][x])
          gapCount[x] += GAP_OVER;
        ++y;
      }
  }
  
  for(int y = 0; y < minHeight; ++y) {
    for(int x = 0; x < width; ++x)
      if(!bitmap[y][x])
        goto nextRow;
    
    rowFullFlags[y] = true;
    continue;
    
  nextRow:
    rowFullFlags[y] = false;
    continue;
  }
}

template<int ww, int hh>
void render_bitmap(Bitmap<ww, hh> &bitmap) {
  for(int y = bitmap.height-1; y >= 0; --y) {
    printf("\033[100m%s", y == bitmap.maxHeight ? "\033[97m__\033[0m" : "  ");
    for(int x = 0; x < bitmap.width; ++x) {
      //printf(bitmap.bitmap[y][x] ? (bitmap.bitmap[y][x] == 2 ? "X" : "x") : (y == 0 ? "_" : " "));
      printf("\033[%sm", bitmap.bitmap[y][x] == 2 ? "106" : (bitmap.bitmap[y][x] ? "47" : "0"));
      if(y == 0)
        printf("%02x", bitmap.gapCount[x]);
      else
        printf("  ");
    }
    printf("\033[100m  \033[0m\n");
  }
}

template<int ww, int hh>
std::string serialize_bitmap(Bitmap<ww, hh> &bitmap, bool invertY = false) {
  std::string out = "";
  
  int y = 0, yEnd = bitmap.height;
  int yStep = 1;
  
  if(!invertY) {
    y = bitmap.height - 1;
    yEnd = -1;
    yStep = -1;
  }
  
  for(; y != yEnd; y += yStep) {
    for(int x = 0; x < bitmap.width; ++x)
      out += std::to_string(bitmap.bitmap[y][x]);
      out += "\n";
    }
  
  return out;
}

template<int w1, int h1, int w2, int h2>
void set_block(Bitmap<w1, h1> &state, Bitmap<w2, h2> &block, int x, int y, uint8_t v) {
  for(int bx = 0; bx < block.width; ++bx) {
    int xPlusBx = x+bx;
    
    for(int by = block.height-1; by >= 0; --by)
      if(block.bitmap[by][bx])
        state.bitmap[y+block.height-by-1][xPlusBx] = v;
    
    int maxY = y + block.height;
    if(state.height_cache[xPlusBx] <= maxY)
      for(int yy = maxY; yy >= 0; --yy)
        if(yy == 0 || state.bitmap[yy-1][xPlusBx]) {
          state.height_cache[xPlusBx] = yy;
          break;
        }
    
    state.gapCount[xPlusBx] = 0;
    for(int yy = 1; yy < state.height_cache[xPlusBx]; ++yy)
      if(!state.bitmap[yy-1][xPlusBx] && state.bitmap[yy][xPlusBx]) {
        state.gapCount[xPlusBx] += GAP_P1;
        if(yy >= 2 && !state.bitmap[yy-2][xPlusBx]) state.gapCount[xPlusBx] += GAP_P2;
        if(yy >= 3 && !state.bitmap[yy-3][xPlusBx]) state.gapCount[xPlusBx] += GAP_P3;
        if(yy+1 < state.height_cache[xPlusBx] && state.bitmap[yy+1][xPlusBx])
          state.gapCount[xPlusBx] += GAP_OVER;
        ++yy;
      }
  }
  
  state.recacheMinMaxHeight();
  
  if(!v) {
    // ATTENTION!
    // I am asserting that every row that this block's bounding box (0,0;width,height) overlapped
    // will now not be full anymore!
    
    for(int yy = y; yy < y+block.height; ++yy)
      state.rowFullFlags[yy] = false;
    return;
  }
  
  for(int yy = y; yy < y+block.height; ++yy) {
    for(int x = 0; x < state.width; ++x)
      if(!state.bitmap[yy][x])
        goto nextRow;
    
    state.rowFullFlags[yy] = true;
    continue;
    
  nextRow:
    // ATTENTION:
    // This row could not have been full before setBlock, because
    // otherwise you shouldn't have called setBlock!
    
    // state.rowFullFlags[yy] = false;
    continue;
  }
}

template<int ww, int hh>
int clear_free_rows(Bitmap<ww, hh> &state) {
  int maxHeight = 0;
  for(int x = 0; x < state.width; ++x)
    if(state.height_cache[x] > maxHeight) maxHeight = state.height_cache[x];
  
  
  int yOff = 0;
  for(int y = 0; y < maxHeight; ++y) {
    for(int x = 0; x < state.width; ++x)
      if(!state.bitmap[y][x])
        goto nextRow;
    
    // this row is free, clear it
    ++yOff;
    
    continue;
    
  nextRow:
    if(yOff)
      memcpy(state.bitmap[y-yOff], state.bitmap[y], sizeof(state.bitmap[0]));
    
    continue;
  }
  
  for(int y = maxHeight - yOff; y < maxHeight; ++y)
    bzero(state.bitmap[y], sizeof(state.bitmap[0]));
  
  state.recache();
  
  return yOff;
}

Bitmap<4, 4> make_block(int id) { // rand() % 7
  int color = id + 1;
  Bitmap<4, 4> block;
  switch(id) {
    case 0: { // line-piece
      for(int y = 0; y < 4; ++y) block.bitmap[y][0] = color;
      block.width = 1;
      block.height = 4;
      block.rotations = 2;
    }; break;
    case 1: { // block
      for(int i = 0; i < 4; ++i) block.bitmap[i&1][i>>1] = color;
      block.width = 2;
      block.height = 2;
      block.rotations = 1;
    }; break;
    case 2: { // T
      block.bitmap[0][0] = color;
      block.bitmap[1][0] = color;
      block.bitmap[1][1] = color;
      block.bitmap[2][0] = color;
      block.width = 2;
      block.height = 3;
      block.rotations = 4;
    }; break;
    case 3: { // L
      block.bitmap[0][0] = color;
      block.bitmap[1][0] = color;
      block.bitmap[2][0] = color;
      block.bitmap[2][1] = color;
      block.width = 2;
      block.height = 3;
      block.rotations = 4;
    }; break;
    case 4: { // inverse L
      block.bitmap[0][1] = color;
      block.bitmap[1][1] = color;
      block.bitmap[2][1] = color;
      block.bitmap[2][0] = color;
      block.width = 2;
      block.height = 3;
      block.rotations = 4;
    }; break;
    case 5: { // z
      block.bitmap[0][1] = color;
      block.bitmap[1][1] = color;
      block.bitmap[1][0] = color;
      block.bitmap[2][0] = color;
      block.width = 2;
      block.height = 3;
      block.rotations = 2;
    }; break;
    case 6: { // inverse z
      block.bitmap[0][0] = color;
      block.bitmap[1][0] = color;
      block.bitmap[1][1] = color;
      block.bitmap[2][1] = color;
      block.width = 2;
      block.height = 3;
      block.rotations = 2;
    }; break;
  }
  
  block.color = color;
  block.recache();
  return block;
}

typedef struct {
  int x, y;
  int rotation;
} block_state_t;

typedef struct {
  Bitmap<4, 4> *block;
  Bitmap<4, 4> *nextBlock;
  
  int depth;
  int completeLines;
  
  block_state_t userAction, bestAction;
  
  double minCost, maxCost, userCost;
} posact_context_t;

template<int ww, int hh>
double possible_actions(Bitmap<ww, hh> &state, posact_context_t &ctx) {
  ctx.minCost = GAMEOVER_COST;
  ctx.maxCost = 0;
  ctx.userCost = 0;
  
  bool willRecurse = ctx.depth < LOOKAHEAD;
  
  posact_context_t ctx2;
  if(willRecurse) {
    ctx2.block = ctx.nextBlock;
    ctx2.nextBlock = NULL;
    ctx2.depth = ctx.depth + 1;
  }
  
  Bitmap<ww, hh> originalState = state;
  for(int i = 0; i < ctx.block->rotations; ++i) { // all rotations
    for(int x = 0; x <= state.width - ctx.block->width; ++x) {
      int maxY = 0;
      for(int bx = 0; bx < ctx.block->width; ++bx) {
        int locY = state.height_cache[x+bx] - (ctx.block->height - ctx.block->height_cache[bx]);
        if(locY > maxY) maxY = locY;
      }
      
      if(x == ctx.userAction.x && i == ctx.userAction.rotation)
        // if the user chose the same rotation and x-value,
        // he can either have the same y-value or he did some intelligent movements
        // that this system does not do, so copy him!
        maxY = ctx.userAction.y;
      
      if(maxY > state.height - ctx.block->height)
        continue;
      
      set_block(state, *ctx.block, x, maxY, true);
      int lineDiff = clear_free_rows(state);
      ctx.completeLines += lineDiff;
      
      double cost = get_cost(state, ctx.completeLines);
      if(cost == GAMEOVER_COST) // don't bother recursing
        willRecurse = false;
      
      if(willRecurse) {
        if(ctx2.block != NULL)
          cost = cost * COST_SPLIT_OWN + possible_actions(state, ctx2) * COST_SPLIT_RECURSION;
        else {
          // we don't know what block is next, so let's try all and see what's worst.
          
          double worstCost = 0;
          
          for(int id = 0; id < 6; ++id) {
            Bitmap<4, 4> possibleBlock = make_block(id);
            ctx2.block = &possibleBlock;
            
            double cost = possible_actions(state, ctx2);
            if(cost > worstCost) worstCost = cost;
          }
          
          cost = cost * COST_SPLIT_OWN + worstCost * COST_SPLIT_RECURSION;
        }
      }
      
      ctx.completeLines -= lineDiff;
      state = originalState;
      
      if(cost == GAMEOVER_COST)
        continue;
      
      if(cost > ctx.maxCost)
        ctx.maxCost = cost;
      
      if(ctx.userAction.x == x && ctx.userAction.y == maxY && ctx.userAction.rotation == i)
        ctx.userCost = cost;
      
      if(cost < ctx.minCost) {
        ctx.minCost = cost;
        ctx.bestAction.x = x;
        ctx.bestAction.y = maxY;
        ctx.bestAction.rotation = i;
      }
    }
    
    // try another rotation
    ctx.block->rotate();
  }
  
  return ctx.minCost;
}

template<int bagSize>
class RandBag {
private:
  int bag[bagSize];
  int index = 0;
  
public:
  void shuffle() {
    for(int i = 0; i < bagSize; ++i) {
      int j = rand() % bagSize;
      
      int tmp = bag[i];
      bag[i] = bag[j];
      bag[j] = tmp;
    }
  }
  
  RandBag() {
    for(int i = 0; i < bagSize; ++i)
      bag[i] = i;
    shuffle();
  }
  
  int next() {
    if(index >= bagSize) {
      shuffle();
      index = 0;
    }
    
    return bag[index++];
  }
};

RandBag<7> randBag;
#define RAND_PIECE_ID (randBag.next())

Bitmap<10, 20> state;
Bitmap<4, 4> block, nextBlock;

int pieceId = 0;
int nextPieceId = 0;

bool gameOverFlag = true;
block_state_t blockState;
std::string serializedState, alternativeState;

void next_piece();
void reset() {
  gameOverFlag = false;
  
  state.reset();
  
  nextPieceId = RAND_PIECE_ID;
  nextBlock = make_block(nextPieceId);
  
  next_piece();
}

std::string render(const block_state_t &action) {
  std::string out;
  
  Bitmap<4, 4> blockCopy = block;
  for(int i = 0; i < action.rotation; ++i)
    blockCopy.rotate();
  
  set_block(state, blockCopy, action.x, action.y, block.color);
  out = serialize_bitmap(state);
  set_block(state, blockCopy, action.x, action.y, 0);
  
  return out;
}

void next_piece() {
  pieceId = nextPieceId;
  block = nextBlock;
  
  nextPieceId = RAND_PIECE_ID;
  nextBlock = make_block(nextPieceId);
  
  blockState.rotation = 0;
  blockState.x = 4;
  blockState.y = state.height - 4;
}

posact_context_t ctx;
void finalize_piece() {
  ctx.userAction = blockState;
  ctx.block = &block;
  ctx.nextBlock = &nextBlock;
  ctx.depth = 0;
  ctx.completeLines = 0;
  
  possible_actions(state, ctx);
  
#ifndef EMSCRIPTEN
  printf("%lf to %lf, you: %lf\n", ctx.minCost, ctx.maxCost, ctx.userCost);
  //sleep(3);
#endif
  
  // - - -
  
  alternativeState = render(ctx.bestAction);
  // std::cout << alternativeState << std::endl;
  
  // - - -
  
  for(int i = 0; i < blockState.rotation; ++i)
    block.rotate();
  
  set_block(state, block, blockState.x, blockState.y, block.color);
  clear_free_rows(state);
}

bool is_move_illegal(block_state_t &blockState) {
  if(blockState.y < 0) return true;
  
  Bitmap<4, 4> blockCopy = block;
  for(int i = 0; i < blockState.rotation; ++i)
    blockCopy.rotate();
  
  if(blockState.x < 0 || blockState.x > state.width-blockCopy.width) return true;
  
  for(int x = 0; x < blockCopy.width; ++x)
    for(int y = 0; y < blockCopy.height; ++y) {
      if(blockCopy.bitmap[y][x] && state.bitmap[blockState.y+blockCopy.height-y-1][x+blockState.x])
        return true;
    }
  
  return false;
}

// 0: normal
// 1: next piece
// 2: game-over
int tick() {
  if(gameOverFlag) return 2;
  
  --blockState.y;
  if(is_move_illegal(blockState)) {
    // cannot move one down, so finalize piece
    ++blockState.y;
    
    finalize_piece();
    next_piece();
    
    if(is_move_illegal(blockState)) {
      gameOverFlag = true;
      return 2;
    }
    
    return 1;
  } else {
    serializedState = render(blockState);
    return 0;
  }
}

void move(short dx) {
  blockState.x += dx;
  if(is_move_illegal(blockState))
    blockState.x -= dx;
  else
    serializedState = render(blockState);
}

void rotate(short dr) {
  block_state_t oldState = blockState;
  
  if(block.height == 4) {
    // line piece, so move the origin
    blockState.x -= dr * ((blockState.rotation & 1) ? +1 : -1);
    blockState.y += dr * ((blockState.rotation & 1) ? +1 : -1);
  }
  
  blockState.rotation += dr;
  blockState.rotation %= block.rotations;
  while(blockState.rotation < 0) blockState.rotation += block.rotations;
  
  if(is_move_illegal(blockState))
    blockState = oldState;
  else
    serializedState = render(blockState);
}

std::string getState() { return serializedState; }
std::string getAlternativeState() { return alternativeState; }

double getMinCost() { return ctx.minCost; }
double getMaxCost() { return ctx.maxCost; }
double getUserCost() { return ctx.userCost; }

void setSeed(int s) {
  srand(s);
#ifdef EMSCRIPTEN
  for(int i = 0; i < (s & 0xffff); ++s)
    rand();
#endif
  
  randBag.shuffle();
}

std::string getNextPiece() { return serialize_bitmap(nextBlock, true); }
bool isGameOver() { return gameOverFlag; }

#ifdef EMSCRIPTEN
#include <emscripten/bind.h>
using namespace emscripten;
#endif

#ifdef EMSCRIPTEN
// JavaScript-API
EMSCRIPTEN_BINDINGS(my_module) {
  function("reset", &reset);
  function("getState", &getState);
  function("getAlternativeState", &getAlternativeState);
  function("tick", &tick);
  function("move", &move);
  function("getMinCost", &getMinCost);
  function("getMaxCost", &getMaxCost);
  function("setSeed", &setSeed);
  function("getNextPiece", &getNextPiece);
  function("isGameOver", &isGameOver);
  function("getUserCost", &getUserCost);
  function("rotate", &rotate);
}
#else
#pragma mark Standalone
int main(int argc, const char *argv[]) {
  srand(9);
  
  reset();
  
  while(true) {
    tick();
    
    if(isGameOver())
      exit(-1);
    
    if(rand() % 4 == 0)
      move(rand() & 1 ? +1 : -1);
    if(rand() % 2 == 0)
      rotate(rand() & 1 ? +1 : -1);
    
    std::cout << serializedState << std::endl;
    std::cout << alternativeState << std::endl;
    usleep(100000);
  }
}
#endif