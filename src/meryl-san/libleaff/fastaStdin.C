
/******************************************************************************
 *
 *  This file is part of canu, a software program that assembles whole-genome
 *  sequencing reads into contigs.
 *
 *  This software is based on:
 *    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' (http://kmer.sourceforge.net)
 *  both originally distributed by Applera Corporation under the GNU General
 *  Public License, version 2.
 *
 *  Canu branched from Celera Assembler at its revision 4587.
 *  Canu branched from the kmer project at its revision 1994.
 *
 *  Modifications by:
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#include "fastaStdin.H"
#include "dnaAlphabets.H"

fastaStdin::fastaStdin(const char *filename) {
  clear();

#ifdef DEBUG
  fprintf(stderr, "fastaStdin::fastaStdin()-- '%s'\n", (filename) ? filename : "NULLPOINTER");
#endif

  if (filename == 0L) {
    strcpy(_filename, "(stdin)");
    _rb = new readBuffer("-");
  } else {

    _pipe = popen(filename, "r");
    _rb = new readBuffer(_pipe);
  }
}


fastaStdin::fastaStdin() {
  clear();
}



fastaStdin::~fastaStdin() {
  delete    _rb;

  if (_pipe)
    pclose(_pipe);

  delete [] _header;
  delete [] _sequence;
}



seqFile *
fastaStdin::openFile(const char *filename) {

#ifdef DEBUG
  fprintf(stderr, "fastaStdin::openFile()-- '%s'\n", (filename) ? filename : "NULLPOINTER");
#endif

  if (((filename == 0L) && (isatty(fileno(stdin)) == 0)) ||
      ((filename != 0L) && (filename[0] == '-') && (filename[1] == 0)))
    return(new fastaStdin(0L));

  if (filename == 0L)
    return(0L);

  //  The stdin variants also handle compressed inputs (because we can't seek in these).

  uint32       fl   = strlen(filename);
  char         cmd[32 + fl];

  if      ((filename[fl-3] == '.') && (filename[fl-2] == 'g') && (filename[fl-1] == 'z'))
    sprintf(cmd, "gzip -dc %s", filename);

  else if ((filename[fl-4] == '.') && (filename[fl-3] == 'b') && (filename[fl-2] == 'z') && (filename[fl-1] == '2'))
    sprintf(cmd, "bzip2 -dc %s", filename);

  else if ((filename[fl-3] == '.') && (filename[fl-2] == 'x') && (filename[fl-1] == 'z'))
    sprintf(cmd, "xz -dc %s", filename);

  else
    return(0L);

  return(new fastaStdin(cmd));
}



uint32
fastaStdin::getNumberOfSequences(void) {
  if (_rb->peek() == 0)
    return(_nextIID);
  else
    return(_nextIID + 1);
}


uint32
fastaStdin::find(const char *sequencename) {
  fprintf(stderr, "fastaStdin::find()-- ERROR!  Used for random access on sequence '%s'.\n", sequencename);
  assert(0);
  return(~uint32ZERO);
}



uint32
fastaStdin::getSequenceLength(uint32 iid) {

  if (iid == _nextIID)
    if (loadNextSequence(_header, _headerLen, _headerMax, _sequence, _sequenceLen, _sequenceMax) == false)
      return(0);

  if (iid + 1 != _nextIID) {
    fprintf(stderr, "fastaStdin::getSequenceLength()-- ERROR!  Used for random access.  Requested iid=%u, at iid=%u\n",
            iid, _nextIID);
    assert(0);
  }

  return(strlen(_sequence));
}



bool
fastaStdin::getSequence(uint32 iid,
                        char *&h, uint32 &hLen, uint32 &hMax,
                        char *&s, uint32 &sLen, uint32 &sMax) {
  bool  ret = true;

#ifdef DEBUG
  fprintf(stderr, "fastaStdin::getSequence(full)-- " F_U32 "\n", iid);
#endif

  if (iid == _nextIID)
    if (loadNextSequence(_header, _headerLen, _headerMax, _sequence, _sequenceLen, _sequenceMax) == false)
      return(false);

  if (iid + 1 != _nextIID) {
    fprintf(stderr, "fastaStdin::getSequence(full)-- ERROR!  Used for random access.  Requested iid=%u, at iid=%u\n",
            iid, _nextIID);
    assert(0);
  }

  if (hLen < _headerMax) {
    delete [] h;
    hMax = _headerMax;
    h    = new char [hMax];
  }

  if (sLen < _sequenceMax) {
    delete [] s;
    sMax = _sequenceMax;
    s    = new char [sMax];
  }

  memcpy(h, _header, _headerLen + 1);
  hLen = _headerLen;

  memcpy(s, _sequence, _sequenceLen + 1);
  sLen = _sequenceLen;

  return(true);
}



bool
fastaStdin::getSequence(uint32 iid,
                        uint32 bgn, uint32 end, char *UNUSED(s)) {

  fprintf(stderr, "fastaStdin::getSequence(part)-- ERROR!  Used for random access on sequence %u bgn %u end %u.\n",
          iid, bgn, end);
  assert(0);
  return(false);
}



void
fastaStdin::clear(void) {
  memset(_filename, 0, FILENAME_MAX);
  memset(_typename, 0, FILENAME_MAX);

  _randomAccessSupported = false;

  strcpy(_typename, "FastAstream");

  _numberOfSequences = ~uint32ZERO;

  _rb          = 0L;
  _nextIID     = 0;
  _pipe        = 0L;

  _header      = 0L;
  _headerLen   = 0;
  _headerMax   = 0;

  _sequence    = 0L;
  _sequenceLen = 0;
  _sequenceMax = 0;
}



bool
fastaStdin::loadNextSequence(char *&h, uint32 &hLen, uint32 &hMax,
                             char *&s, uint32 &sLen, uint32 &sMax) {

  if (hMax == 0) {
    hMax = 2048;
    h    = new char [hMax];
  }

  if (sMax == 0) {
    sMax = 2048;
    s    = new char [sMax];
  }

  hLen = 0;
  sLen = 0;

  char   x   = _rb->read();

  //  Skip whitespace at the start of the sequence.
  while ((_rb->eof() == false) && (alphabet.isWhitespace(x) == true))
    x = _rb->read();

  //  We should be at a '>' character now.  Fail if not.
  if (_rb->eof() == true)
    return(false);
  if (x != '>')
    fprintf(stderr, "fastaStdin::loadNextSequence(part)-- ERROR: In %s, expected '>' at beginning of defline, got '%c' instead.\n",
            _filename, x), exit(1);

  //  Skip the '>' in the defline
  x = _rb->read();

  //  Skip whitespace between the '>' and the defline
  while ((_rb->eof() == false) && (alphabet.isWhitespace(x) == true) && (x != '\r') && (x != '\n'))
    x = _rb->read();

  //  Copy the defline, until the first newline.
  while ((_rb->eof() == false) && (x != '\r') && (x != '\n')) {
    h[hLen++] = x;
    if (hLen >= hMax) {
      //fprintf(stderr, "realloc header\n");
      hMax += 2048;
      char *H = new char [hMax];
      memcpy(H, h, hLen);
      delete [] h;
      h = H;
    }
    x = _rb->read();
  }
  h[hLen] = 0;

  //  Skip whitespace between the defline and the sequence.
  while ((_rb->eof() == false) && (alphabet.isWhitespace(x) == true))
    x = _rb->read();

  //  Copy the sequence, until EOF or the next '>'.
  while ((_rb->eof() == false) && (_rb->peek() != '>')) {
    if (alphabet.isWhitespace(x) == false) {
      s[sLen++] = x;
      if (sLen >= sMax) {
        //fprintf(stderr, "realloc sequence\n");
        sMax *= 2;
        char *S = new char [sMax];
        memcpy(S, s, sLen);
        delete [] s;
        s = S;
      }
    }
    x = _rb->read();
  }
  s[sLen] = 0;

  _nextIID++;

  return(true);
}
