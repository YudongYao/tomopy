#!/usr/bin/env python
# -*- coding: utf-8 -*-

# #########################################################################
# Copyright (c) 2015, UChicago Argonne, LLC. All rights reserved.         #
#                                                                         #
# Copyright 2015. UChicago Argonne, LLC. This software was produced       #
# under U.S. Government contract DE-AC02-06CH11357 for Argonne National   #
# Laboratory (ANL), which is operated by UChicago Argonne, LLC for the    #
# U.S. Department of Energy. The U.S. Government has rights to use,       #
# reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR    #
# UChicago Argonne, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR        #
# ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is     #
# modified to produce derivative works, such modified software should     #
# be clearly marked, so as not to confuse it with the version available   #
# from ANL.                                                               #
#                                                                         #
# Additionally, redistribution and use in source and binary forms, with   #
# or without modification, are permitted provided that the following      #
# conditions are met:                                                     #
#                                                                         #
#     * Redistributions of source code must retain the above copyright    #
#       notice, this list of conditions and the following disclaimer.     #
#                                                                         #
#     * Redistributions in binary form must reproduce the above copyright #
#       notice, this list of conditions and the following disclaimer in   #
#       the documentation and/or other materials provided with the        #
#       distribution.                                                     #
#                                                                         #
#     * Neither the name of UChicago Argonne, LLC, Argonne National       #
#       Laboratory, ANL, the U.S. Government, nor the names of its        #
#       contributors may be used to endorse or promote products derived   #
#       from this software without specific prior written permission.     #
#                                                                         #
# THIS SOFTWARE IS PROVIDED BY UChicago Argonne, LLC AND CONTRIBUTORS     #
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       #
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS       #
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UChicago     #
# Argonne, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,        #
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,    #
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;        #
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER        #
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT      #
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN       #
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE         #
# POSSIBILITY OF SUCH DAMAGE.                                             #
# #########################################################################

from __future__ import absolute_import, division, print_function

from test.util import read_file
from tomopy.recon.algorithm import *
import numpy as np
from nose.tools import assert_equals
from numpy.testing import assert_array_almost_equal


__author__ = "Doga Gursoy"
__copyright__ = "Copyright (c) 2015, UChicago Argonne, LLC."
__docformat__ = 'restructuredtext en'


class TestRecon:

    def __init__(self):
        self.prj = read_file('proj.npy')
        self.ang = read_file('angle.npy')

    def test_art(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='art', num_iter=4),
            read_file('art.npy'))

    def test_bart(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='bart', num_iter=4),
            read_file('bart.npy'))

    def test_fbp(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='fbp'),
            read_file('fbp.npy'))

    # def test_gridrec(self):
    #     assert_array_almost_equal(
    #         recon(self.prj, self.ang, algorithm='gridrec'),
    #         read_file('gridrec.npy'))

    def test_mlem(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='mlem', num_iter=4),
            read_file('mlem.npy'))

    def test_osem(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='osem', num_iter=4),
            read_file('osem.npy'))

    def test_ospml_hybrid(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='ospml_hybrid', num_iter=4),
            read_file('ospml_hybrid.npy'))

    def test_ospml_quad(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='ospml_quad', num_iter=4),
            read_file('ospml_quad.npy'))

    def test_pml_hybrid(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='pml_hybrid', num_iter=4),
            read_file('pml_hybrid.npy'))

    def test_pml_quad(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='pml_quad', num_iter=4),
            read_file('pml_quad.npy'))

    def test_sirt(self):
        assert_array_almost_equal(
            recon(self.prj, self.ang, algorithm='sirt', num_iter=4),
            read_file('sirt.npy'))


if __name__ == '__main__':
    import nose
    nose.runmodule(exit=False)
