
#include <assert.h>
#include <time.h>
#include <vector>
#include <string>
using namespace std;

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_gfxPrimitives.h"

#define NPOP 2

#define MUTATION_ADD 100
#define MUTATION_MOVE 200
#define MUTATION_DEL 200
#define MUTATION_COLOUR 200
#define MUTATION_SIZE 400

#define TRAND(x) if(!(rand() % (MUTATION_##x / 2)))
#define RANDINT(a, b) ((rand() % (b-a + 1)) + a)

SDL_Surface *target;
SDL_Surface *screen;
SDL_Surface *test;

long long int diff(SDL_Surface *a, SDL_Surface *b){
  Uint8 *da, *db;
  da = (Uint8 *)a->pixels;
  db = (Uint8 *)b->pixels;
  long long int delta;
  for (int i = 0; i < a->w * a->h; i++){
    long long int tmp2;
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
  return delta;
}

struct Gene{
  int x, y, w;
  Uint8 c[4];
  Gene(){
    x = rand() % target->w;
    y = rand() % target->h;
    w = 10;
    c[0] = rand() % 256;
    c[1] = rand() % 256;
    c[2] = rand() % 256;
    c[3] = rand() % 256;
  }
  void draw(SDL_Surface *s){
    filledCircleRGBA(s, x, y, w, c[0], c[1], c[2], c[3]);
  }
  void mutate(){
    TRAND(MOVE){
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
      w += RANDINT(-3, 3);
    }
    for(int i = 0; i < 4; i++){
      TRAND(COLOUR){
        c[i] = rand() % 256;
      }
      TRAND(COLOUR){
        c[i] += RANDINT(-20, 20);
      }
    }
  }
};

struct Genome{
  vector<Gene> genes;
  void draw(SDL_Surface *s){
    SDL_FillRect(s, NULL, 0x00aaaaaa);
    for(int i = 0; i < genes.size(); i++){
      genes[i].draw(s);
    }
  }
  void mutate(){
    TRAND(ADD){
      genes.push_back(Gene());
    }
    if(!genes.empty()){
      TRAND(DEL){
        genes.push_back(Gene());
      }
    }
    for(int i = 0; i < genes.size(); i++){
      genes[i].mutate();
    }
  }
  int size(){
    return genes.size();
  }
};

Genome genomes[NPOP];

void run(){
  for(int iter = 0;; iter++){
    genomes[0].draw(test);
    long long int best = diff(test, target);
    int bestidx = 0;
    for(int i = 1; i < NPOP; i++){
      genomes[i].draw(test);
      long long int f = diff(test, target);
      if(f < best){
        best = f;
        bestidx = i;
      }
    }
    genomes[0] = genomes[bestidx];
    genomes[0].draw(screen);
    SDL_Flip(screen);
    printf("Iteration %i\tFitness: %lli\tCircles: %i\n", iter, best, genomes[bestidx].size());
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

int main(int argc, char *argv[]){
  srand(time(NULL));
  SDL_Init(SDL_INIT_VIDEO);
  target = IMG_Load("mona.bmp");
  screen = SDL_SetVideoMode(target->w, target->h, 32, SDL_SWSURFACE);
  target = SDL_DisplayFormat(target);
  test = SDL_CreateRGBSurface(SDL_SWSURFACE, target->w, target->h, target->format->BitsPerPixel, target->format->Rmask, target->format->Gmask, target->format->Bmask, target->format->Amask);
  run();
  return 0;
}

