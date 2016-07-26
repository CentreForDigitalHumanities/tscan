#ifndef UTILS_H
#define	UTILS_H

std::istream& safe_getline( std::istream&, std::string& );

struct proportion {
  proportion( double d1, double d2 ) {
    if ( d2 == 0 || isnan(d1) || isnan(d2) )
      p = NAN;
    else
      p = d1/d2;
  };
  double p;
};

struct density {
  density( double d1, double d2 ) {
    if ( d2 == 0 || isnan(d1) || isnan(d2) )
      d = NAN;
    else
      d = (d1/d2) * 1000;
  };
  double d;
};

std::ostream& operator<<(std::ostream&, const proportion&);
std::ostream& operator<<(std::ostream&, const density&);

#endif /* UTILS_H */