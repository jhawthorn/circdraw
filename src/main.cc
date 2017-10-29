
#include <assert.h>
#include <time.h>
#include <vector>
#include <string>
using namespace std;

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_gfxPrimitives.h"

#define DEPTH 32
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

#define NPOP 2
#define N_GENES 40

#define MUTATION_ADD 200
#define MUTATION_MOVE 200
#define MUTATION_DEL 200
#define MUTATION_COLOUR 200
#define MUTATION_SIZE 400
#define MUTATION_SWAP 400

#define TRAND(x) if(!(rand() % (MUTATION_##x)))
#define RANDINT(a, b) ((rand() % (b-a + 1)) + a)

SDL_Surface *target;
SDL_Surface *screen;
SDL_Surface *test;

long long int diff(SDL_Surface *a, SDL_Surface *b){
  Uint8 *da, *db;

  SDL_LockSurface(a);
  SDL_LockSurface(b);

  da = (Uint8 *)a->pixels;
  db = (Uint8 *)b->pixels;
  long long int delta = 0;
  for (int i = 0; i < a->w * a->h; i++){
    long long int tmp2 = 0;
    int tmp = (*da++) - (*db++);
    tmp2 += tmp * tmp;
    tmp = (*da++) - (*db++);
    tmp2 += tmp * tmp;
    tmp = (*da++) - (*db++);
    tmp2 += tmp * tmp;
    delta += sqrt(tmp2);
    da++;
    db++;
  }

  SDL_UnlockSurface(b);
  SDL_UnlockSurface(a);

  return delta;
}

struct Gene{
  int x, y, r;
  Uint8 c[4];
  Gene(){
    x = rand() % target->w;
    y = rand() % target->h;
    r = 10;
    c[0] = rand() % 256;
    c[1] = rand() % 256;
    c[2] = rand() % 256;
    c[3] = 255;
  }

  void draw(SDL_Surface *s){
    filledCircleRGBA(s, x, y, r, c[0], c[1], c[2], c[3]);
  }

  bool mutate(){
    bool dirty = false;
    TRAND(MOVE){
      dirty = true;
      switch(rand() % 3){
        case 0:
          x = rand() % target->w;
          y = rand() % target->h;
          break;
        case 1:
          x += RANDINT(-20, 20);
          y += RANDINT(-20, 20);
          break;
        case 2:
          x += RANDINT(-3, 3);
          y += RANDINT(-3, 3);
          break;
      }
    }
    TRAND(SIZE){
      dirty = true;
      r += RANDINT(-3, 3);
    }
    for(int i = 0; i < 3; i++){
      TRAND(COLOUR){
        dirty = true;
        c[i] = rand() % 256;
      }
      TRAND(COLOUR){
        dirty = true;
        c[i] += RANDINT(-20, 20);
      }
    }
    return dirty;
  }
};

struct Genome{
  Gene genes[N_GENES];
  long long int _score;
  bool dirty;

  Genome(){
    dirty = true;
  }

  void draw(SDL_Surface *s){
    SDL_FillRect(s, NULL, 0x00aaaaaa);
    for(int i = 0; i < N_GENES; i++){
      genes[i].draw(s);
    }
  }

  void mutate(){
    for(int i = 0; i < N_GENES; i++){
      if(genes[i].mutate()) {
        dirty = true;
      }
      TRAND(SWAP){
        dirty = true;
        int j = RANDINT(0,N_GENES);
        Gene tmp = genes[j];
        genes[j] = genes[i];
        genes[i] = tmp;
      }
    }
  }

  long long int score() {
    if(dirty) {
      draw(test);
      _score = diff(test, target);
      dirty = false;
    }
    return _score;
  }

  int size(){
    return N_GENES;
  }
};

struct Run{
  Genome genomes[NPOP];

  void run(){
    for(int iter = 0;; iter++){
      long long int best = genomes[0].score();
      int bestidx = 0;
      for(int i = 1; i < NPOP; i++){
        long long int f = genomes[i].score();
        if(f < best){
          best = f;
          bestidx = i;
        }
      }
      genomes[0] = genomes[bestidx];

      if((iter % 100) == 0) {
        genomes[0].draw(screen);
        SDL_UpdateRect(screen, 0, 0, 0, 0);
        printf("Iteration %i\tFitness: %lli\tCircles: %i\n", iter, best, genomes[bestidx].size());
      }

      for(int i = 1; i < NPOP; i++){
        genomes[i] = genomes[bestidx];
        genomes[i].draw(test);
        assert(genomes[i].size() == genomes[bestidx].size());
        genomes[i].mutate();
      }
      SDL_Event e;
      while(SDL_PollEvent(&e)){
        if(e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
          return;
        }
      }
    }
  }
};

int main(int argc, char *argv[]){
  SDL_Init(SDL_INIT_VIDEO);
  target = IMG_Load("mona.bmp");
  screen = SDL_SetVideoMode(target->w, target->h, DEPTH, SDL_SWSURFACE);

  test = SDL_CreateRGBSurface(SDL_SWSURFACE, target->w, target->h, DEPTH, RMASK, GMASK, BMASK, AMASK);

  target = SDL_ConvertSurface(target, test->format, SDL_SWSURFACE);

  (new Run())->run();
  return 0;
}

