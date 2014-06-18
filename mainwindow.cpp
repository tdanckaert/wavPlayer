#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdio.h>
#include <iostream>
#include <iterator>

#include <QFileDialog>
#include <QShortcut>
#include <QTimer>
#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QApplication>
#include <QDesktopWidget>

#include <sndfile.hh>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
using std::vector;
using std::set;
using std::cerr;
using std::endl;

enum PlayEvent {
  PAUSE
};

enum PlayState {
  PLAYING,
  STOPPED
};

static bool haveSample;
set<vector<float>>::iterator curSample;
static int channels;
static jack_port_t *outputPort;
static jack_client_t *client;
static unsigned int playbackIndex;
static PlayState state;
static jack_ringbuffer_t *eventBuffer;

static jack_ringbuffer_t *inQueue; // samples in
static jack_ringbuffer_t *outQueue; // samples out, can be freed

static int process(jack_nframes_t nframes, void* )
{
  // read and process incoming events
  set<vector<float>>::iterator i;
  while(jack_ringbuffer_read_space(inQueue) >= sizeof(i) ) {
    jack_ringbuffer_read(inQueue, (char*)&i, sizeof(i) );
    if(haveSample && curSample != i 
       && jack_ringbuffer_write_space(outQueue) >= sizeof(curSample)) {
      jack_ringbuffer_write(outQueue, (char*)&curSample, sizeof(curSample));
    }
    curSample = i;
    haveSample = true;
    playbackIndex = 0;
  }

  while(jack_ringbuffer_read_space(eventBuffer) >= sizeof(PlayEvent) ) {
    PlayEvent e;
    jack_ringbuffer_read( eventBuffer, (char*)&e, sizeof(PlayEvent) );
    switch(e) {
    case PAUSE:
      switch(state) {
      case PLAYING:
        state = STOPPED;
        break;
      case STOPPED:
        state = haveSample ? PLAYING : STOPPED;
        break;
      }
      break;
    }
  }

  float* outputBuffer= (float*)jack_port_get_buffer ( outputPort, nframes);

  // this is the intresting part, we work with each sample of audio data
  // one by one, copying them across from the vector that has the sample
  // in it, to the output buffer of JACK.
    switch(state) {
    case PLAYING:
      for (jack_nframes_t i = 0; i < nframes; ++i)
        {
          // here we check if index has gone played the last sample, and if it
          // has, then we reset it to 0 (the start of the sample).
          if ( playbackIndex >= curSample->size() ) {
            playbackIndex = 0;
          }
    
          // because we have made sure that index is always between 0 -> sample.size(),
          // its safe to access the array .at(index)  The program would crash it furthur
          outputBuffer[i] = curSample->at(playbackIndex++);
          
          for(int j=1; j<channels; ++j) {
            outputBuffer[i] += curSample->at(playbackIndex++);
          }
          outputBuffer[i] /= channels;
        }
      break;
    case STOPPED:
      for (jack_nframes_t i=0; i < nframes; ++i) {
        outputBuffer[i] = 0.;
      }
      break;
    }
  
  return 0;
}

void MainWindow::drawWave(const set<vector<float> >::iterator &sample) {
  auto scene = ui->waveOverview->scene();
  scene->clear();

  auto height = 0.6 *  QApplication::desktop()->screenGeometry().height();
  vector<QPointF> points;
  points.reserve(sample->size());
  float ampl = 0.5*height;
  for(unsigned int i=0; i<sample->size(); ++i) {
    points.push_back(QPointF(i/150.0, ampl*sample->at(i) +ampl ) );
  }

  auto map = QPixmap(sample->size()/150.0, height);
  map.fill();

  QPainter painter(&map);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

  painter.drawPolyline(&points[0], points.size());
  auto item = scene->addPixmap(map);

  ui->waveOverview->fitInView(item);
  ui->waveOverview->setSceneRect(item->boundingRect());
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  ui->waveOverview->setScene(new QGraphicsScene(this));

  auto shortcutPlay = new QShortcut(QKeySequence(Qt::Key_Space), this);
  connect(shortcutPlay, SIGNAL(activated()), this, SLOT(pause()));

  auto timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(cleanup()));
  timer->start(500);
  
  playbackIndex = 0;
  outputPort = nullptr;
  
  client = jack_client_open("wavPlayer", JackNullOption, 0 , 0);
  if (client == nullptr) {
    cerr << __func__ << " client failed!" << endl;
  }

  size_t ringbufferSize = sizeof(PlayEvent)*10;
  
  eventBuffer = jack_ringbuffer_create(ringbufferSize);
  jack_ringbuffer_mlock(eventBuffer);

  inQueue = jack_ringbuffer_create(sizeof(set<vector<float>>::iterator)*10);
  jack_ringbuffer_mlock(inQueue);
  outQueue = jack_ringbuffer_create(sizeof(set<vector<float>>::iterator)*10);
  jack_ringbuffer_mlock(outQueue);
  
  outputPort = jack_port_register ( client,
                                    "output",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput,
                                    0 );

  jack_set_process_callback  (client, process , 0);
  
  state = STOPPED;
  haveSample = false;

  jack_activate(client);

  jack_connect(client, "wavPlayer:output", "system:playback_1");
  jack_connect(client, "wavPlayer:output", "system:playback_2");
}

MainWindow::~MainWindow()
{
    delete ui;
    jack_deactivate(client);
    jack_ringbuffer_free(eventBuffer);
    jack_ringbuffer_free(inQueue);
    jack_ringbuffer_free(outQueue);
    jack_client_close(client);
}

void MainWindow::on_actionQuit_triggered()
{
    QCoreApplication::quit();
}

vector<float> openSoundFile(QString fileName) {

  vector<float> result;

  // create a "Sndfile" handle, it's part of the sndfile library we 
  // use to load samples
  SndfileHandle fileHandle( fileName.toUtf8().data() , SFM_READ,  SF_FORMAT_WAV | SF_FORMAT_FLOAT , 1 , 44100);
  
  // get the number of frames in the sample
  int size  = fileHandle.frames();

  // handle size 0?
  
  // get some more info of the sample
  channels   = fileHandle.channels();
  int samplerate = fileHandle.samplerate();
  
  result.resize(channels*size);
  
  // this tells sndfile to 
  fileHandle.read( &result.at(0) , channels*size );
  
  qDebug() << "Loaded a file with " << channels << " channels, and a samplerate of " <<
    samplerate << " with " << size << " samples, so its duration is " <<
    size / samplerate << " seconds long.";
  
  return result;
}

void MainWindow::cleanup(void) {
  set<vector<float> >::iterator i;
  while(jack_ringbuffer_read_space(outQueue) >= sizeof(i) ) {
    jack_ringbuffer_read(outQueue, (char*)&i, sizeof(i) );
    qDebug() << __func__ << ": erasing sample";
    samples.erase(i);
 }
}

void MainWindow::on_actionOpen_triggered()
{
  auto fileName = QFileDialog::getOpenFileName();
  
  if(!fileName.isEmpty()) {
    auto iSample = samples.insert(openSoundFile(fileName));
    if(iSample.first->size() 
       && jack_ringbuffer_write_space(inQueue) >= sizeof(iSample.first)) {
      jack_ringbuffer_write(inQueue, (const char *)&iSample.first, sizeof(iSample.first));
      drawWave(iSample.first);
    } else {
      samples.erase(iSample.first);
    }
  }
}

void MainWindow::pause(void) {
  PlayEvent e = PAUSE;
  if(jack_ringbuffer_write_space(eventBuffer) >= sizeof(PlayEvent)) {
    jack_ringbuffer_write(eventBuffer, (const char*)&e, sizeof(PlayEvent));
  } else {
    cerr << "Can't write to eventBuffer" << endl;
  }
}

void MainWindow::on_splitter_splitterMoved(int pos, int index)
{
  QGraphicsItem *item = nullptr;

  auto sample = samples.begin();
  if(sample == samples.end())
    return;

  auto height = ui->waveOverview->frameRect().height();

  item = ui->waveOverview->scene()->items().first();

  ui->waveOverview->fitInView(item);
}
