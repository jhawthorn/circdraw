
#include <assert.h>
#include <time.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
using namespace std;

#include <opencv2/opencv.hpp>
using namespace cv;

#define PARENTS 16
#define NPOP 128
#define N_GENES 128

#define ALLOW_ALPHA 0

#define MUTATION_ADD 200
#define MUTATION_MOVE 200
#define MUTATION_DEL 200
#define MUTATION_COLOUR 200
#define MUTATION_SIZE 400
#define MUTATION_SWAP 400
#define MUTATION_RANDOM 400

#define TRAND(x) if(!(rand() % (MUTATION_##x)))
#define RANDINT(a, b) ((rand() % (b-a + 1)) + a)

struct Dimensions {
  int w, h;
};

Dimensions dim = {100, 100};

double diff(const Mat &a, const Mat &b){
  Mat dst;

  absdiff(a, b, dst);
  dst.convertTo(dst, CV_32F);
  dst = dst.mul(dst);

  Scalar s = sum(dst);
  return s[0] + s[1] + s[2];
}

struct Gene{
  int x, y, r;
  unsigned char c[4];

  Gene() {
    randomize();
  }

  void draw(Mat &image){
    if(r < 0) {
      return;
    }
    Point center = {x, y};
    circle(
        image,
        center,
        r,
        Scalar(c[2], c[1], c[0]),
        FILLED,
        LINE_8 );
  }

  void randomize() {
    x = rand() % dim.w;
    y = rand() % dim.h;
    r = RANDINT(10,50);
    c[0] = rand() % 256;
    c[1] = rand() % 256;
    c[2] = rand() % 256;
    c[3] = ALLOW_ALPHA ? (rand() % 256) : 255;
  }

  bool mutate(){
    bool dirty = false;
    TRAND(RANDOM){
      dirty = true;
      randomize();
    }
    TRAND(MOVE){
      dirty = true;
      switch(rand() % 3){
        case 0:
          x = rand() % dim.w;
          y = rand() % dim.h;
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
    for(int i = 0; i < (ALLOW_ALPHA ? 4 : 3); i++){
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

  void writeSVG(std::ostream &o) const {
    o << "<circle ";
    o << "cx=\"" << x << "\" cy=\"" << y << "\" r=\"" << r << "\" ";
    o << "fill=\"#" << std::hex << (int)c[0] << (int)c[1] << (int)c[2] << (int)c[3] << std::dec << "\" ";
    o << " />" << std::endl;
  }
};

struct Genome{
  Gene genes[N_GENES];
  double _score;
  bool dirty;

  Genome(){
    dirty = true;
  }

  void offspringFrom(const Genome &a, const Genome &b){
    for(int i = 0; i < N_GENES; i++) {
      if(rand() % 2) {
        genes[i] = a.genes[i];
      } else {
        genes[i] = b.genes[i];
      }
    }
    dirty = true;
  }

  void draw(Mat &image){
    for(int i = 0; i < N_GENES; i++){
      genes[i].draw(image);
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

  double score() const {
    return _score;
  }

  void recalculate(const Mat &targetImage) {
    Mat testImage = Mat::zeros(targetImage.size(), CV_8UC3);
    if(dirty) {
      draw(testImage);

      _score = diff(testImage, targetImage);
      dirty = false;
    }
  }

  int size(){
    return N_GENES;
  }

  bool operator< (const Genome &other) const {
    return score() < other.score();
  }

  bool operator== (const Genome &other) const {
    return score() == other.score();
  }

  void writeSVG(std::ostream &o) const {
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" " << std::endl;
    o << "width=\"" << dim.w << "\" height=\"" << dim.h << "\">" << std::endl;
    for(int i = 0; i < N_GENES; i++) {
      genes[i].writeSVG(o);
    }
    o << "</svg>" << std::endl;
  }
};

struct Population {
  Genome genomes[NPOP];

  const Genome& best() {
    return genomes[0];
  }

  void prune() {
    std::sort(std::begin(genomes), std::end(genomes));
    std::unique(std::begin(genomes), std::end(genomes));
  }

  void recalculate(const Mat &targetImage) {
    for(int i = 0; i < NPOP; i++)
      genomes[i].recalculate(targetImage);
  }

  void mutate() {
    for(int i = PARENTS; i < NPOP; i++){
      int parenta = rand() % PARENTS;
      int parentb = rand() % PARENTS;
      genomes[i].offspringFrom(genomes[parenta], genomes[parentb]);
      genomes[i].mutate();
    }
  }

  void step(const Mat &targetImage) {
    recalculate(targetImage);
    prune();
    mutate();
  }
};


struct Run{
  char *input_filename;
  std::string output_filename;
  Population *population;

  Mat targetImage;

  Run(char *filename) {
    input_filename = filename;

    output_filename.append(input_filename);
    output_filename.append(".svg");

    targetImage = imread(input_filename, IMREAD_COLOR);

    dim = {targetImage.size().width, targetImage.size().height};

    population = new Population();
  }

  void run(){
    for(int iter = 0;; iter++){
      population->step(targetImage);

      if((iter % 10) == 0) {
        cout << "Iteration " << iter << "\tParents: ";
        for(int i = 0; i < PARENTS; i++) {
          cout << " " << population->genomes[i].score();
        }
        cout << endl;

        std::ofstream file;
        file.open(output_filename);
        population->best().writeSVG(file);
        file.close();
      }
    }
  }
};

void usage(int argc, char *argv[]) {
  printf("USAGE: %s IMAGE.bmp\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[]){
  if(argc != 2) {
    usage(argc, argv);
  }
  (new Run(argv[1]))->run();
  return 0;
}

