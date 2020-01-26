#!/usr/bin/env python
#-*- coding:utf-8 -*-

import sys
import os
import site


import tscanservice.tscan #** import your configuration module here! **
import clam.clamservice
application = clam.clamservice.run_wsgi(tscanservice.tscan) #** pass your module to CLAM **

