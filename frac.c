/* mandelbrot parallel fractal generator
 * Copyright (C) 2007 Christopher Wellons (mosquitopsu@gmail.com)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "mandel.h"
#include "common.h"
#include "colormap.h"
#include "Image.h"

/* Calculated log (2) beforehand */
double logtwo = 0.693147180559945;

/* Output options */
int write_text = 0;		/* Write textfile */
int write_bmp = 1;		/* Write bitmap */
int write_pipe = 0;		/* Write data through pipe to parent */
int pipe_fid = 0;		/* Pipe to parent */

FTYPE *gen_mandel_p (int width, int height,
		     double xmin, double xmax,
		     double ymin, double ymax, int it, int jobs)
{
  /* Prepare job handling data */
  int *job_height = xmalloc (jobs * sizeof (int));
  int jobn, htotal = 0;
  for (jobn = 0; jobn < jobs; jobn++)
    {
      job_height[jobn] = ceil (height / jobs);
      if (htotal + job_height[jobn] > height)
	job_height[jobn] = height - htotal;

      htotal += job_height[jobn];
    }
  double job_y = (ymax - ymin) / jobs;
  int *read_pipe = xmalloc ((jobs + 1) * sizeof (int));

  /* Generate fractal */
  int i;
  for (i = 0; i < jobs; i++)
    {
      /* Communication pipe */
      pipe (read_pipe + i);
      pipe_fid = read_pipe[i + 1];

      pid_t fork_out = fork ();
      if (fork_out < 0)
	fprintf (stderr, "Failed to fork\n");

      if (fork_out == 0)
	{
	  FTYPE *img = xmalloc (width * job_height[i] * sizeof (FTYPE));
	  img = gen_mandel (width, job_height[i],
			    xmin, xmax,
			    ymin + job_y * i, ymin + job_y * (i + 1), it);

	  /* Write result to parent */
	  write_bmp = 0;
	  write_text = 0;
	  write_pipe = 1;
	  write_data (img, width, job_height[i], it);

	  exit (EXIT_SUCCESS);
	}
    }

  /* Parent collects data */
  FTYPE *img = xmalloc (width * height * sizeof (FTYPE));
  int loc = 0;
  for (i = 0; i < jobs; i++)
    {
      int job_size = (width * job_height[i]);
      read (read_pipe[i], (img + loc), job_size * sizeof (FTYPE));
      loc += job_size;
      printf ("Job done: %d\n", i + 1);
    }

  free (job_height);
  free (read_pipe);

  return img;
}

int write_data (FTYPE * img, int w, int h, int it)
{
  if (write_text)
    {
      FILE *fout = fopen ("out.dat", "w");
      int i, j;
      for (j = 0; j < h; j++)
	{
	  for (i = 0; i < w; i++)
	    fprintf (fout, FTYPE_STR, img[i + j * w]);
	  fprintf (fout, "\n");
	}
      fclose (fout);
    }

  if (write_bmp)
    {
      Image bmp = createImage (w, h);
      int i, j;
      for (j = 0; j < h; j++)
	for (i = 0; i < w; i++)
	  imageSetPixelRgb (bmp, i, j, colormap (img[i + j * w], it));
      saveImage (bmp, "out.bmp");
      destroyImage (bmp);
    }

  if (write_pipe)
    {
      write (pipe_fid, img, w * h * sizeof (FTYPE));
    }

  return 0;
}

FTYPE *gen_mandel (int width, int height,
		   double xmin, double xmax, double ymin, double ymax, int it)
{
  double xres = (xmax - xmin) / width;
  double yres = (ymax - ymin) / height;

  /* Fractal data */
  FTYPE *img = (FTYPE *) xmalloc (width * height * sizeof (FTYPE));

  int i, j;
  for (j = 0; j < height; j++)
    for (i = 0; i < width; i++)
      img[i + j * width] = get_val (xmin + i * xres, ymin + j * yres, it);

  return img;
}

FTYPE get_val (double creal, double cimag, int it)
{
  /* Init Z */
  double zreal = 0;
  double zimag = 0;

  int count = 0;
  double ztemp;
  while ((zreal * zreal + zimag * zimag < 2 * 2) && count < it)
    {
      /* Z = Z^2 + C */
      ztemp = zreal * zreal - zimag * zimag + creal;
      zimag = 2 * zreal * zimag + cimag;
      zreal = ztemp;

      count++;
    }
  if (zreal * zreal + zimag * zimag < 2 * 2)
    return 0;

  /* Smooth coloration */
  FTYPE out = (FTYPE) (count + 1 -
		       log (log (sqrt (zreal * zreal + zimag * zimag))) /
		       logtwo);
  return out;
}
