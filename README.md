[![Actions Status](https://github.com/CentreForDigitalHumanities/tscan/actions/workflows/cpp.yml/badge.svg)](https://github.com/CentreForDigitalHumanities/tscan/actions/workflows/cpp.yml) [![Actions Status](https://github.com/CentreForDigitalHumanities/tscan/actions/workflows/webservice.yml/badge.svg)](https://github.com/CentreForDigitalHumanities/tscan/actions/workflows/webservice.yml) [![DOI](https://zenodo.org/badge/36359165.svg)](https://zenodo.org/badge/latestdoi/36359165)

# T-Scan

tscan 0.10 (c) TiCC/ 1998 - 2023

    Tilburg centre for Cognition and Communication, Tilburg University.
    UiL-OTS, Utrecht University
    Language Machines, Centre for Language Studies, Nijmegen

T-Scan is distributed under the GNU Affero Public Licence (see the file COPYING).

T-Scan is an analysis tool for dutch texts to assess the complexity of the
text, and is based on original work by Rogier Kraf (Utrecht University) (see:
Kraf et al., 2009). The code has been reimplemented and extended by Ko van der
Sloot (Tilburg University), Martijn van der Klis (Utrecht University) and is currently maintained and continued by Luka van der Plas and Sheean Spoel (both Utrecht University).

## Web application / Webservice

This repository contains the T-Scan source code, allowing you to run it
yourself on your own system. In addition, T-Scan is available as a web application and webservice through https://tscan.hum.uu.nl/tscan/. You can [create an account](https://tmanage.hum.uu.nl/accounts/signup/) or [contact us](https://dig.hum.uu.nl/contact-and-links/) if your institution is not (yet) recognized.

## Documentation

Extensive documentation (in Dutch) can be found in [``docs/tscanhandleiding.pdf``](./docs/tscanhandleiding.pdf).

## Installation

T-Scan heavily depends upon other software, such as Frog, Wopr and Alpino.

Installation is not trivial, to be able to successfully build T-Scan from the tarball, you need the following packages:
- autotools
- autoconf-archive
- [ticcutils](https://github.com/LanguageMachines/ticcutils)
- [libfolia](https://github.com/LanguageMachines/libfolia)
- [Alpino](https://www.let.rug.nl/vannoord/alp/Alpino/)
- [frog](https://github.com/LanguageMachines/frog)
- [wopr](https://github.com/LanguageMachines/wopr)
- [CLAM](https://github.com/proycon/clam)

We strongly recommend to use Docker to install T-scan. Be aware that T-scan and dependencies are memory intensive, we recommend at least 16GB RAM for proper operation. If WOPR is used (which is enabled by default!) more RAM is required: 32 GB is recommended.

### Docker

This version of T-Scan can run directly from Docker:

    $ docker compose up

Default address: http://localhost:8830

To speed up rebuilds the Dockerfile makes extensive use of caching. The following can be found in `docker/data`:

* `build-cache`: this contains the output of the compiled C++ code, helps speed up a rebuild where the code didn't change
* `compound-dependencies`: dependencies for the compound splitter, nearly 820 MB which you really don't want to have to download again on every rebuild
* `compound-dependencies/dist`: the Python package for the [compound splitter](https://github.com/UUDigitalHumanitieslab/compound-splitter)
* `packages`: the prebuilt dependencies (Frog, Ucto, etc)

Only the `build-cache` has automatic invalidation, if you want to update your dependencies you need to delete (parts) of this cache. The cache will be automatically recreated during startup.

### Manual Build

If you do not want to use (the provided) dockerfile, first make sure you have **all** necessary dependencies and then compile/install as follows:

    $ bash bootstrap.sh
    $ ./configure --prefix=/path/to/installation/
    $ make
    $ sudo make install
    $ cd webservice
    $ sudo python3 setup.py install

## Usage

Before you can use T-Scan you need to start the background servers (you may need to edit the scripts to set ports and paths):

    $ cd tscan/webservice
    $ ./startalpino.sh
    $ ./startfrog.sh
    $ ./startwopr20.sh    (will start Wopr to calculate forwards probabilities)
    $ ./startwopr02.sh    (will start Wopr to calculate backwards probabilities)

Then either run T-Scan from the command-line, which will produce a FoLiA XML file,

    $ cd tscan
    $ cp tscan.cfg.example tscan.cfg
    (edit tscan.cfg if necessary)
    $ tscan --config=tscan.cfg input.txt

... or use the webapplication/webservice, which you can start with:

    $ cd tscan/webservice/tscanservice
    $ clamservice tscanservice.tscan   #this starts the CLAM service for T-Scan

And then navigate to the host and port specified.

## Tests

Tests can be run using `make check`. This requires running the Frog services:

```bash
cd webservice
./startfrog.sh &
```

Pre-parsed Alpino files are included. It is also possible to remove these and update them for a newer version of Alpino.

```bash
./startalpino.sh &
cd ../tests/
rm alpino_lookup.data
rm *.alpino
./testall
./merge_alpino_output.py
```

Note: the output can change when a different version of Alpino or Frog is used.

## Data

[Word prevalence values](http://crr.ugent.be/programs-data/word-prevalence-values) (in `data/prevalence_nl.data` and `data/prevalence_be.data`) courtesy of Keuleers et al., Center for Reading Research, Ghent University.

Certain parts of T-Scan use data from [Referentiebestand Nederlands](http://tst.inl.nl/producten/rbn/), which we can not distribute due to restrictive licensing issues, so this functionality will not be available.

Certain other data is too large for GitHub, but will be downloaded for you automatically by the ``./downloaddata.sh`` script.

## References

* Kraf, R. & Pander Maat, H. (2009). Leesbaarheidsonderzoek: oude problemen en nieuwe kansen. *Tijdschrift voor Taalbeheersing* 31(2), 97-123.
* Pander Maat, H. & Kraf, R. & van den Bosch, A. & Dekker, N. & van Gompel, M. & Kleijn, S. & Sanders, T. & van der Sloot, K. (2014). T-Scan: a new tool for analyzing Dutch text. *Computational Linguistics in the Netherlands Journal* 4, 53-74.
* Keuleers, E. & Stevens, M. & Mandera, P. & Brysbaert, M. (2015). Word knowledge in the crowd: Measuring vocabulary size and word prevalence in a massive online experiment. *The Quarterly Journal of Experimental Psychology* 68(8), 1665-1692.
