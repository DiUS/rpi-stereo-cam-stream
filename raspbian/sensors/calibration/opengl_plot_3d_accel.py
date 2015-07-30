#!/usr/bin/python
""" Plots X Y Z """

import os
current_dir = os.path.dirname(os.path.realpath(__file__))
import sys
import fileinput
import numpy

from pyqtgraph.Qt import QtCore, QtGui
import pyqtgraph.opengl as gl


def calibrate_ellipsoid(x, y, z):
    H = numpy.array([x, y, z, -y**2, -z**2, numpy.ones([len(x), 1])])
    H = numpy.transpose(H)
    w = x**2

    (X, residues, rank, shape) = numpy.linalg.lstsq(H, w)

    OSx = X[0] / 2
    OSy = X[1] / (2 * X[3])
    OSz = X[2] / (2 * X[4])

    A = X[5] + OSx**2 + X[3] * OSy**2 + X[4] * OSz**2
    B = A / X[3]
    C = A / X[4]

    SCx = numpy.sqrt(A)
    SCy = numpy.sqrt(B)
    SCz = numpy.sqrt(C)

    g = 9.80665

    return ([-OSx, -OSy, -OSz], [1/SCx * g, 1/SCy * g, 1/SCz * g])


def calibrate_min_max(x, y, z):
    xmin = numpy.amin(x)
    xmax = numpy.amax(x)
    ymin = numpy.amin(y)
    ymax = numpy.amax(y)
    zmin = numpy.amin(z)
    zmax = numpy.amax(z)

    x_offset = -((xmin+xmax)/2.0)
    y_offset = -((ymin+ymax)/2.0)
    z_offset = -((zmin+zmax)/2.0)

    si_xmax = xmax + x_offset
    si_xmin = xmin + x_offset
    si_ymax = ymax + y_offset
    si_ymin = ymin + y_offset
    si_zmax = zmax + z_offset
    si_zmin = zmin + z_offset
    avgs_x = (abs(si_xmax) + (abs(si_xmin))) / 2.0
    avgs_y = (abs(si_ymax) + (abs(si_ymin))) / 2.0
    avgs_z = (abs(si_zmax) + (abs(si_zmin))) / 2.0
    avg_rad = (avgs_x + avgs_y + avgs_z) / 3.0
    x_scale = (avg_rad/avgs_x);
    y_scale = (avg_rad/avgs_y);
    z_scale = (avg_rad/avgs_z);

    return ([x_offset, y_offset, z_offset], [x_scale, y_scale, z_scale])


def print_usage(prog_cmd):
    print "Usage: %s [input_file]" % prog_cmd


def run():
    infile_list = sys.argv[1:]

    if len(infile_list) == 0:
        print_usage(sys.argv[0])
        sys.exit(-1)

    for infile in infile_list:
        if not os.path.isfile(infile):
            print "Error: data file \"%s\" not found." % infile
            sys.exit(-1)

    # prepare data
    datasets = []
    for infile in infile_list:
        dset = {'name': '', 'xs': [], 'ys': [], 'zs': []}
        datasets.append(dset)
        dset['name'] = os.path.basename(os.path.splitext(infile)[0]).title()

        f = fileinput.input([infile])
        for line in f:
            tokens = line.split()
            if len(tokens) == 0:
                continue
            if len(tokens) != 3:
                print "Error: unexpected input data in file %s:%d" % (infile, f.filelineno())
                sys.exit(-1)

            xs = float(tokens[0])
            ys = float(tokens[1])
            zs = float(tokens[2])
            dset['xs'].append(xs)
            dset['ys'].append(ys)
            dset['zs'].append(zs)

    # calibration
    for d in datasets:
        # find min max
        xs = numpy.array(d['xs'])
        ys = numpy.array(d['ys'])
        zs = numpy.array(d['zs'])

        (offsets, scales) = calibrate_ellipsoid(xs, ys, zs)

        x_offset = offsets[0]
        y_offset = offsets[1]
        z_offset = offsets[2]
        print 'x offset = %f' % x_offset
        print 'y offset = %f' % y_offset
        print 'z offset = %f' % z_offset
        x_scale = scales[0]
        y_scale = scales[1]
        z_scale = scales[2]
        print 'x scale  = %f' % x_scale
        print 'y scale  = %f' % y_scale
        print 'z scale  = %f' % z_scale

        # Calibrate reading
        d['cxs'] = map(lambda x: (x+x_offset)*x_scale, d['xs'])
        d['cys'] = map(lambda y: (y+y_offset)*y_scale, d['ys'])
        d['czs'] = map(lambda z: (z+z_offset)*z_scale, d['zs'])

        # old calibration routine
        (offsets_mm, scales_mm) = calibrate_min_max(xs, ys, zs)
        mm_x_offset = offsets_mm[0]
        mm_y_offset = offsets_mm[1]
        mm_z_offset = offsets_mm[2]
        print 'mm x offset = %f' % mm_x_offset
        print 'mm y offset = %f' % mm_y_offset
        print 'mm z offset = %f' % mm_z_offset
        mm_x_scale = scales_mm[0]
        mm_y_scale = scales_mm[1]
        mm_z_scale = scales_mm[2]
        print 'mm x scale  = %f' % mm_x_scale
        print 'mm y scale  = %f' % mm_y_scale
        print 'mm z scale  = %f' % mm_z_scale
        # Calibrate reading
        d['mmcxs'] = map(lambda x: (x+mm_x_offset)*mm_x_scale, d['xs'])
        d['mmcys'] = map(lambda y: (y+mm_y_offset)*mm_y_scale, d['ys'])
        d['mmczs'] = map(lambda z: (z+mm_z_offset)*mm_z_scale, d['zs'])


    app = QtGui.QApplication([])
    w = gl.GLViewWidget()
    w.opts['distance'] = 50
    w.show()
    w.setWindowTitle('Sensor Calibration Plot')
    
    g = gl.GLGridItem()
    # WORKAROUND: set scale to workaround setSize problem
    g.scale(2, 2, 2)
    w.addItem(g)

    sp1 = gl.GLScatterPlotItem(pos=numpy.array(zip(xs, ys, zs)), color=(1,0,0,.5), size=2)
    w.addItem(sp1)

    cxs = numpy.array(d['cxs'])
    cys = numpy.array(d['cys'])
    czs = numpy.array(d['czs'])
    sp2 = gl.GLScatterPlotItem(pos=numpy.array(zip(cxs, cys, czs)), color=(0,1,0,.5), size=2)
    w.addItem(sp2)

    mmcxs = numpy.array(d['mmcxs'])
    mmcys = numpy.array(d['mmcys'])
    mmczs = numpy.array(d['mmczs'])
    sp3 = gl.GLScatterPlotItem(pos=numpy.array(zip(mmcxs, mmcys, mmczs)), color=(0,0,1,.5), size=2)
    w.addItem(sp3)

    QtGui.QApplication.instance().exec_()




if __name__ == "__main__":
    run()
