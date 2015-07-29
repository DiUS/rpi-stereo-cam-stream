#!/usr/bin/python
""" Plots Radius X Y Z """

import os
current_dir = os.path.dirname(os.path.realpath(__file__))
import sys
import fileinput
import math
import numpy

import matplotlib as mpl
import matplotlib.pyplot as plt




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

        dset['xmin'] = numpy.amin(xs)
        dset['xmax'] = numpy.amax(xs)
        dset['ymin'] = numpy.amin(ys)
        dset['ymax'] = numpy.amax(ys)
        dset['zmin'] = numpy.amin(zs)
        dset['zmax'] = numpy.amax(zs)

        print 'x min max = (%f, %f)' % (dset['xmin'], dset['xmax'])
        print 'y min max = (%f, %f)' % (dset['ymin'], dset['ymax'])
        print 'z min max = (%f, %f)' % (dset['zmin'], dset['zmax'])

        # Hard Iron offsets
        x_offset = -((d['xmin']+d['xmax'])/2.0)
        y_offset = -((d['ymin']+d['ymax'])/2.0)
        z_offset = -((d['zmin']+d['zmax'])/2.0)
        print 'x offset = %f' % x_offset
        print 'y offset = %f' % y_offset
        print 'z offset = %f' % z_offset
        # Soft Iron scale
        si_xmax = d['xmax'] + x_offset
        si_xmin = d['xmin'] + x_offset
        si_ymax = d['ymax'] + y_offset
        si_ymin = d['ymin'] + y_offset
        si_zmax = d['zmax'] + z_offset
        si_zmin = d['zmin'] + z_offset
        avgs_x = (abs(si_xmax) + (abs(si_xmin))) / 2.0
        avgs_y = (abs(si_ymax) + (abs(si_ymin))) / 2.0
        avgs_z = (abs(si_zmax) + (abs(si_zmin))) / 2.0
        avg_rad = (avgs_x + avgs_y + avgs_z) / 3.0
        x_scale = (avg_rad/avgs_x);
        y_scale = (avg_rad/avgs_y);
        z_scale = (avg_rad/avgs_z);
        print 'x scale  = %f' % x_scale
        print 'y scale  = %f' % y_scale
        print 'z scale  = %f' % z_scale
        # Calibrate reading
        d['cxs'] = map(lambda x: (x+x_offset)*x_scale, d['xs'])
        d['cys'] = map(lambda y: (y+y_offset)*y_scale, d['ys'])
        d['czs'] = map(lambda z: (z+z_offset)*z_scale, d['zs'])
        # Radius
        d['radius'] = map(lambda (x, y, z): math.sqrt(math.pow(x, 2) + math.pow(y, 2) + math.pow(z, 2)), zip(d['xs'], d['ys'], d['zs']))
        d['radius_offset'] = map(lambda (x, y, z): math.sqrt(math.pow(x+x_offset, 2) + math.pow(y+y_offset, 2) + math.pow(z+z_offset, 2)), zip(d['xs'], d['ys'], d['zs']))
        d['radius_scaled'] = map(lambda (x, y, z): math.sqrt(math.pow(x, 2) + math.pow(y, 2) + math.pow(z, 2)), zip(d['cxs'], d['cys'], d['czs']))
        #d['nxs'] = map(lambda (x,r): (x+x_offset)/r, zip(d['xs'],d['radius_offset']))
        #d['nys'] = map(lambda (y,r): (y+y_offset)/r, zip(d['ys'],d['radius_offset']))
        #d['nzs'] = map(lambda (z,r): (z+z_offset)/r, zip(d['zs'],d['radius_offset']))
        #d['radius_normalized'] = map(lambda (x, y, z): math.sqrt(math.pow(x+x_offset, 2) + math.pow(y+y_offset, 2) + math.pow(z+z_offset, 2)), zip(d['nxs'], d['nys'], d['nzs']))
        # average and stddev
        nparr = numpy.array(d['radius'])
        d['mean'] = numpy.mean(nparr, dtype=numpy.float64)
        d['stddev'] = numpy.std(nparr, dtype=numpy.float64)
        print 'mean   = %f' % d['mean']
        print 'stddev = %f' % d['stddev']
        num_std_dev = 2.0
        lower_range = d['mean']-d['stddev']*num_std_dev
        upper_range = d['mean']+d['stddev']*num_std_dev
        print 'range (%f stdev) = (%f - %f)' % (num_std_dev, lower_range, upper_range)
        for idx, r in enumerate(d['radius']):
            if r <= lower_range or r >= upper_range:
                print 'sample %d (%f, %f, %f) radius %f' % (idx+1, d['xs'][idx], d['ys'][idx], d['zs'][idx], r)


    fig = plt.figure()
    
    # plot
    ax = fig.add_subplot(1, 1, 1)
    ax.xaxis.grid(True, which='both')
    ax.yaxis.grid(True)
    ax.minorticks_on()
    ax.set_title('Radius')
    ax.set_ylabel('radius')
    plegend = []
    slegend = []
    for d in datasets:
        ptmp, = ax.plot(d['radius'])
        plegend.append(ptmp)
        slegend.append(d['name'] + ' raw')
        ptmp, = ax.plot(d['radius_offset'])
        plegend.append(ptmp)
        slegend.append(d['name'] + ' offset')
        ptmp, = ax.plot(d['radius_scaled'])
        plegend.append(ptmp)
        slegend.append(d['name'] + ' scaled')

        #ptmp, = ax.plot(d['xs'])
        #plegend.append(ptmp)
        #slegend.append(d['name'] + ' X')
        #ptmp, = ax.plot(d['ys'])
        #plegend.append(ptmp)
        #slegend.append(d['name'] + ' Y')
        #ptmp, = ax.plot(d['zs'])
        #plegend.append(ptmp)
        #slegend.append(d['name'] + ' Z')
    ax.legend(plegend, slegend)

    plt.show()






if __name__ == "__main__":
    run()
