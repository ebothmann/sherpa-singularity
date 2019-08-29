// usage: ./HepMCReader -a <Rivet-analysis-1> -a <rivet-analysis-2> ... -o <output-file-base-name> <input-file-1> <input-file-2> ...

#include "HepMC/IO_GenEvent.h"
#include "HepMC/GenEvent.h"
#include "HepMC/WeightContainer.h"
#include "HepMC/GenCrossSection.h"
#include "HepMC/HepMCDefs.h"

#include "Rivet/AnalysisHandler.hh"
#include "Rivet/Tools/Logging.hh"

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <vector>
#include <zlib.h>
#include <cmath>

// -------------------------------------------------------------------------
// Internal classes to implement gzstream. See below for user classes.
// -------------------------------------------------------------------------

class gzstreambuf : public std::streambuf {
private:
  static const int bufferSize = 47+256;    // size of data buff
  // totals 512 bytes under g++ for igzstream at the end.

  gzFile           file;               // file handle for compressed file
  char             buffer[bufferSize]; // data buffer
  char             opened;             // open/close state of stream
  int              mode;               // I/O mode

  int flush_buffer() {
    // Separate the writing of the buffer from overflow() and
    // sync() operation.
    int w = pptr() - pbase();
    if ( gzwrite( file, pbase(), w) != w)
      return EOF;
    pbump( -w);
    return w;
  }

public:
  gzstreambuf() : opened(0) {
    setp( buffer, buffer + (bufferSize-1));
    setg( buffer + 4,     // beginning of putback area
	  buffer + 4,     // read position
	  buffer + 4);    // end position
    // ASSERT: both input & output capabilities will not be used together
  }
  ~gzstreambuf() { close(); }

  int is_open() { return opened; }
  gzstreambuf* open( const char* name, int open_mode) {
    if ( is_open())
      return (gzstreambuf*)0;
    mode = open_mode;
    // no append nor read/write mode
    if ((mode & std::ios::ate) || (mode & std::ios::app)
	|| ((mode & std::ios::in) && (mode & std::ios::out)))
      return (gzstreambuf*)0;
    char  fmode[10];
    char* fmodeptr = fmode;
    if ( mode & std::ios::in)
      *fmodeptr++ = 'r';
    else if ( mode & std::ios::out)
      *fmodeptr++ = 'w';
    *fmodeptr++ = 'b';
    *fmodeptr = '\0';
    file = gzopen( name, fmode);
    if (file == 0)
      return (gzstreambuf*)0;
    opened = 1;
    return this;
  }
  gzstreambuf* close() {
    if ( is_open()) {
      sync();
      opened = 0;
      if ( gzclose( file) == Z_OK)
	return this;
    }
    return (gzstreambuf*)0;
  }

  virtual int underflow() { // used for input buffer only
    if ( gptr() && ( gptr() < egptr()))
      return * reinterpret_cast<unsigned char *>( gptr());

    if ( ! (mode & std::ios::in) || ! opened)
      return EOF;
    // Josuttis' implementation of inbuf
    int n_putback = gptr() - eback();
    if ( n_putback > 4)
	n_putback = 4;
    memcpy( buffer + (4 - n_putback), gptr() - n_putback, n_putback);

    int num = gzread( file, buffer+4, bufferSize-4);
    if (num <= 0) // ERROR or EOF
      return EOF;

    // reset buffer pointers
    setg( buffer + (4 - n_putback),   // beginning of putback area
	  buffer + 4,                 // read position
	  buffer + 4 + num);          // end of buffer

    // return next character
    return * reinterpret_cast<unsigned char *>( gptr());
  }
  virtual int overflow( int c) { // used for output buffer only
    if ( ! ( mode & std::ios::out) || ! opened)
      return EOF;
    if (c != EOF) {
      *pptr() = c;
      pbump(1);
    }
    if ( flush_buffer() == EOF)
      return EOF;
    return c;
  }
};

class gzstreambase : virtual public std::ios {
protected:
    gzstreambuf buf;
public:
    gzstreambase( const char* name, int mode) {
      init( &buf);
      open( name, mode);
    }
    ~gzstreambase() { buf.close(); }
    void open( const char* name, int open_mode) {
      if ( ! buf.open( name, open_mode))
	clear( rdstate() | std::ios::badbit);
    }
    void close() {
      if ( buf.is_open())
	if ( ! buf.close())
	  clear( rdstate() | std::ios::badbit);
    }
};

class igzstream : public gzstreambase, public std::istream {
public:
    igzstream( const char* name, int open_mode = std::ios::in)
        : gzstreambase( name, open_mode), std::istream( &buf) {}
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace HepMC2RivetParser {
  class RivetVariation {
  private:
    std::string m_name;
    Rivet::AnalysisHandler *m_rivet;
    double m_wgt, m_n, m_sum, m_sum2;
  public:
    RivetVariation(std::string name="", Rivet::AnalysisHandler *rivet=NULL)
      : m_name(name), m_rivet(rivet),
        m_wgt(0.), m_n(0.), m_sum(0.), m_sum2(0.) {}
    ~RivetVariation() { if (m_rivet) delete m_rivet; }

    void AddEvent(const double& wgt, const double& n) {
      m_wgt=wgt;
      m_n+=n;
      m_sum+=wgt;
      m_sum2+=wgt*wgt;
    }
    double TotalXS() const {
      if (m_n==0.) return 0.;
      return m_sum/m_n;
    }
    double TotalErr() const {
      if (m_n<=1.) return TotalXS();
      return sqrt((m_sum2-m_sum*m_sum/m_n)/(m_n-1.)/m_n);
    }

    inline Rivet::AnalysisHandler * Rivet() { return m_rivet; }

    inline std::string Name() const         { return m_name; }
    inline double      Weight() const       { return m_wgt; }
    inline double      SumOfWeights() const { return m_sum; }
  }; // end of class RivetVariation

  typedef std::map<std::string,RivetVariation *> RivetVariationMap;


#ifdef HEPMC_HAS_NAMED_WEIGHTS
  void InitVariations(const HepMC::WeightContainer& wc,
                      RivetVariationMap& rvm,
                      const std::vector<std::string>& anas,
                      size_t& mode)
  {
    std::vector<std::string> keys;
#ifdef HEPMC_HAS_WORKING_NAMED_WEIGHTS
    keys=wc.keys();
#else
    // since weights' names cannot currently be accessed,
    // inspect output stream
    std::stringstream str;
    str.precision(16);
    wc.print(str);
    while (str) {
      std::string cur("");
      str>>cur;
      if (cur.length()==0) continue;
      // weight is between "," and trailing bracket
      // name is between leading bracket and ","
      cur=cur.substr(1,cur.find(",")-1);
      keys.push_back(cur);
    }
#endif
    for (size_t i(0);i<keys.size();++i) {
      std::string key(keys[i]);
      if ((key=="0") ||
          (key=="Weight") ||
          (key=="weight") ||
          (key.find("MUR")!=std::string::npos &&
           key.find("MUF")!=std::string::npos &&
           key.find("PDF")!=std::string::npos)) {
        Rivet::AnalysisHandler *rivet(new Rivet::AnalysisHandler());
        rivet->addAnalyses(anas);
        rvm[key]=new RivetVariation(key,rivet);
      }
      else if (key=="NTrials") mode=1;
    }
  }
#endif

} // end of namespace HepMC2RivetParser

using namespace std;
using namespace HepMC;
using namespace HepMC2RivetParser;

int main(int argc, char* argv[]) {
#ifdef HEPMC_HAS_NAMED_WEIGHTS
  std::vector<std::string> args(argv, argv+argc);
  std::vector<std::string> filenames;
  std::vector<std::string> analyses;
  std::string              outfilename("analysis");

  // read input
  for (size_t i(1);i<args.size();++i) {
    if (args[i].substr(0,2)=="-a") { analyses.push_back(args[i+1]); ++i; }
    else if (args[i].substr(0,2)=="-o") { outfilename=args[i+1]; ++i; }
    else filenames.push_back(argv[i]);
  }

  // output
  std::string str;
  for (size_t i(0);i<analyses.size();++i) {
    str.append(analyses[i]);
    if (i<analyses.size()-1) str.append(", ");
  }
  std::cout<<"Analsyes: "<<str<<std::endl;
  std::cout<<"Input file names: ";
  for (size_t i(0);i<filenames.size();++i) {
    std::cout<<filenames[i]<<std::endl;
  }
  std::cout<<"Output file name: "<<outfilename<<std::endl;

  // analyse
  RivetVariationMap rvm;
  size_t n(0), nmode(0);
  for (size_t i(0);i<filenames.size();++i) {
    std::cout<<filenames[i]<<std::endl;

    igzstream istr(filenames[i].c_str());
    HepMC::IO_GenEvent ascii_readin(istr);
    HepMC::GenEvent* evt;
    while ((evt=ascii_readin.read_next_event())) {
      if (n==0) InitVariations(evt->weights(),rvm,analyses,nmode);
      // extract ntrials
      double ntrial(nmode?evt->weights()["NTrials"]:1.);
      for (RivetVariationMap::iterator it(rvm.begin());it!=rvm.end();++it) {
        double wgt(evt->weights()[it->first]);
        it->second->AddEvent(wgt,ntrial);
        // since the bloody WeightContainer cannot be replaced,
        // replace the first element
        evt->weights()[0]=wgt;
        // need to also replace GenCrossSection object
        GenCrossSection xs;
        xs.set_cross_section(it->second->TotalXS(),it->second->TotalErr());
        evt->set_cross_section(xs);
        // feed into Rivet now
        it->second->Rivet()->analyze(evt);
      }
      ++n;
      if (n%10==0) std::cout<<"Analysed "<<n<<" events."<<std::endl;
      delete evt; evt=NULL;
    }
  }

  // finalise
  for (RivetVariationMap::iterator it(rvm.begin());it!=rvm.end();++it) {
    std::cout<<it->first<<": ( "<<it->second->TotalXS()<<" +- "
                                <<it->second->TotalErr()<<" ) pb"<<std::endl;
    it->second->Rivet()->finalize();
    std::string variation_tag((it->first == "Weight") ? "" : "."+it->first);
    it->second->Rivet()->writeData(outfilename+variation_tag+".yoda");
    delete it->second;
  }
#else
  std::error<<"Does not work if HepMC does not support named weights"<<std::endl;
  abort();
#endif
  std::cout<<"Done."<<std::endl;
  return 0;
}
