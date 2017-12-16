
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

#define MUTATION_MOVE 200
#define MUTATION_COLOUR 200
#define MUTATION_SIZE 400
#define MUTATION_SWAP 400
#define MUTATION_RANDOM 400

#define TRAND(x) if(!(rand() % (MUTATION_##x * 100)))
#define RANDINT(a, b) ((rand() % (b-a + 1)) + a)

#define RANDF() ((float)rand()/(float)(RAND_MAX))

#include "comparators.h"

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

  void writeSVG(std::ostream &o, const Size size) const {
    char rgb[7];
    sprintf(rgb, "%.2X%.2X%.2X", c[0], c[1], c[2]);
    o << "<circle ";
    o << "cx=\"" << x*size.width << "\" cy=\"" << y*size.height << "\" r=\"" << r*size.width << "\" ";
    o << "fill=\"#" << rgb << "\" ";
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

  void draw(Mat &image) const {
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
    Mat testImage(target.size(), CV_8UC3, cv::Scalar(255,255,255));
    if(target.justChanged || dirty) {
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

  void writeSVG(std::ostream &o, const Size size) const {
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" " << std::endl;
    o << "width=\"100%\" height=\"100%\" ";
    o << "viewBox=\"0 0 " << size.width << " " << size.height << "\" ";
    o << "preserveAspectRatio=\"xMidYMid meet\" ";
    o << ">" << std::endl;
    for(int i = 0; i < N_GENES; i++) {
      genes[i].writeSVG(o, size);
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

  VideoCapture cap;

  Run(): target(NULL), cap(0) {
    captureFrame();

    target = new MSEComparator(targetImage);

    population = new Population();
  }

  void captureFrame() {
    cap >> targetImage;
    resize(targetImage, targetImage, Size(240, 180));
    if(target)
      target->setTarget(targetImage);
  }

  void run(){
    for(int iter = 0;; iter++){
      population->step(*target);

      target->justChanged = false;

      if((iter % 10) == 0) {
        captureFrame();

        cout << "Iteration " << iter << "\tParents: ";
        for(int i = 0; i < PARENTS; i++) {
          cout << " " << population->genomes[i].score();
        }
        cout << endl;

        namedWindow("Capture", WINDOW_AUTOSIZE);
        imshow("Capture", target->getTarget());

        namedWindow("Paint", WINDOW_AUTOSIZE);
        Mat preview(target->size(), CV_8UC3, cv::Scalar(255,255,255));
        population->best().draw(preview);
        imshow("Paint", preview);

        Mat diff;
        absdiff(target->getTarget(), preview, diff);
        namedWindow("diff", WINDOW_AUTOSIZE);
        imshow("diff", diff);

        int delay = 10;
        char c = (char)cvWaitKey(delay);
        if (c == 27) break;
      }
    }
  }
};

void usage(int argc, char *argv[]) {
  cout << "USAGE: " << argv[0] << endl;
  exit(1);
}

int main(int argc, char *argv[]){
  if(argc != 1) {
    usage(argc, argv);
  }
  (new Run())->run();
  return 0;
}

