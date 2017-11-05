
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

#define RANDF() ((float)rand()/(float)(RAND_MAX))

struct Dimensions {
  int w, h;
};

Dimensions dim = {100, 100};

struct Comparator {
  Mat original;

  Comparator(Mat &o) {
    original = o;
  }

  double diff(const Mat &other) const {
    Mat dst;

    absdiff(original, other, dst);
    dst.convertTo(dst, CV_16UC3);
    dst = dst.mul(dst);

    Scalar s = sum(dst);
    return s[0] + s[1] + s[2];
  }
};

struct Gene{
  float x, y, r;
  unsigned char c[4];

  Gene() {
    randomize();
  }

  void draw(Mat &image) const {
    if(r < 0) {
      return;
    }
    Size size = image.size();
    Point center = {(int)(x * size.width), (int)(y * size.height)};
    circle(
        image,
        center,
        r * size.width,
        Scalar(c[2], c[1], c[0]),
        FILLED,
        LINE_8 );
  }

  void randomize() {
    x = RANDF();
    y = RANDF();
    r = RANDF() / 10;
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
          x = RANDF();
          y = RANDF();
          break;
        case 1:
          x += (RANDF() - 0.5) / 10;
          x += (RANDF() - 0.5) / 10;
          break;
        case 2:
          x += (RANDF() - 0.5) / 500;
          x += (RANDF() - 0.5) / 500;
          break;
      }
    }
    TRAND(SIZE){
      dirty = true;
      r += ((RANDF() - 0.5) / 10);
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

  void recalculate(const Comparator &target) {
    Mat testImage = Mat::zeros(target.original.size(), CV_8UC3);
    if(dirty) {
      draw(testImage);

      _score = target.diff(testImage);
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

  void recalculate(const Comparator &target) {
    parallel_for_(Range(0, NPOP), [&](const Range& range){
      for(int i = range.start; i < range.end; i++) {
        genomes[i].recalculate(target);
      }
    });
  }

  void mutate() {
    for(int i = PARENTS; i < NPOP; i++){
      int parenta = rand() % PARENTS;
      int parentb = rand() % PARENTS;
      genomes[i].offspringFrom(genomes[parenta], genomes[parentb]);
      genomes[i].mutate();
    }
  }

  void step(const Comparator &target) {
    recalculate(target);
    prune();
    mutate();
  }
};


struct Run{
  char *input_filename;
  std::string output_filename;
  Population *population;

  Mat targetImage;
  Comparator *target;

  Run(char *filename) {
    input_filename = filename;

    output_filename.append(input_filename);
    output_filename.append(".svg");

    targetImage = imread(input_filename, IMREAD_COLOR);
    target = new Comparator(targetImage);

    dim = {targetImage.size().width, targetImage.size().height};

    population = new Population();
  }

  void run(){
    for(int iter = 0;; iter++){
      population->step(*target);

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
  cout << "USAGE: " << argv[0] << " IMAGEFILE" << endl;
  exit(1);
}

int main(int argc, char *argv[]){
  if(argc != 2) {
    usage(argc, argv);
  }
  (new Run(argv[1]))->run();
  return 0;
}

