/*--------------------------------------------------------------*/
/* node.c -- Generation	of detailed network and obstruction	*/
/* information on the routing grid based on the geometry of the	*/
/* layout of the standard cell macros.				*/
/*								*/
/*--------------------------------------------------------------*/
/* Written by Tim Edwards, June, 2011, based on work by Steve	*/
/* Beccue.							*/
/*--------------------------------------------------------------*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "qrouter.h"
#include "node.h"
#include "config.h"
#include "lef.h"

/*--------------------------------------------------------------*/
/* create_netorder --- assign indexes to net->netorder    	*/
/*								*/
/* 	Nets are ordered simply from those with the most nodes	*/
/*	to those with the fewest.  However, any nets marked	*/
/* 	critical in the configuration or critical net files	*/
/*	will be given precedence.				*/
/*								*/
/*  ARGS: none							*/
/*  RETURNS: nothing						*/
/*  SIDE EFFECTS: Nlnets -> netorder is assigned		*/
/*--------------------------------------------------------------*/

void create_netorder()
{
  int i, max;
  NET  net;
  STRING cn;

  i = 1;
  for (cn = CriticalNet; cn; cn = cn->next) {
     fprintf(stdout, "critical net %s\n", cn->name);
     for (net = Nlnets; net; net = net->next) {
	if (!strcmp(net->netname, (char *)cn->name)) {
           net->netorder = i++;
	}
     }
  }

  for (; i <= Numnets; i++) {
     max = 0;

     for (net = Nlnets; net; net = net->next) {
	if (!net->netorder) {
	   if (net->numnodes > max) max = net->numnodes;
	}
     }

     for (net = Nlnets; net; net = net->next) {
	if (!net->netorder) {
	   if (net->numnodes == max) {
	      net->netorder = i;
	      break;
	   }
        }
     }
  }
} /* create_netorder() */

/*--------------------------------------------------------------*/
/* print_nodes - show the nodes list				*/
/*         ARGS: filename to print to
        RETURNS: nothing
   SIDE EFFECTS: none
AUTHOR and DATE: steve beccue      Tue Aug 04  2003
\*--------------------------------------------------------------*/

void print_nodes(char *filename)
{
  FILE *o;
  int i;
  NET net;
  NODE node;
  DPOINT dp;

    if (!strcmp(filename, "stdout")) {
	o = stdout;
    } else {
	o = fopen(filename, "w");
    }
    if (!o) {
	fprintf( stderr, "node.c:print_nodes.  Couldn't open output file\n" );
	return;
    }

    for (net = Nlnets; net; net = net->next) {
       for (node = net->netnodes; node; node = node->next) {
	  dp = (DPOINT)node->taps;
	  fprintf(o, "%d\t%s\t(%g,%g)(%d,%d) :%d:num=%d netnum=%d\n",
		node->nodenum, 
		node->netname,
		// legacy:  print only the first point
		dp->x, dp->y, dp->gridx, dp->gridy,
		node->netnum, node->numnodes, node->netnum );
		 
	  /* need to print the routes to this node (deprecated)
	  for (i = 0 ; i < g->nodes; i++) {
	      fprintf(o, "%s(%g,%g) ", g->node[i], *(g->x[i]), *(g->y[i]));
	  }
	  */
       }
    }
    fclose(o);

} /* void print_nodes() */

/*--------------------------------------------------------------*/
/*C print_nlnets - show the nets				*/
/*         ARGS: filename to print to
        RETURNS: nothing
   SIDE EFFECTS: none
AUTHOR and DATE: steve beccue      Tue Aug 04  2003
\*--------------------------------------------------------------*/

void print_nlnets( char *filename )
{
  FILE *o;
  int i;
  NODE nd;
  NET net;

    if (!strcmp(filename, "stdout")) {
	o = stdout;
    } else {
	o = fopen(filename, "w");
    }
    if (!o) {
	fprintf(stderr, "node.c:print_nlnets.  Couldn't open output file\n");
	return;
    }

    for (net = Nlnets; net; net = net->next) {
	fprintf(o, "%d\t#=%d\t%s   \t\n", net->netnum, 
		 net->numnodes, net->netname);

	for (nd = net->netnodes; nd; nd = nd->next) {
	   fprintf(o, "%d ", nd->nodenum);
	}
    }

    fprintf(o, "%d nets\n", Numnets);
    fflush(o);

} /* void print_nlnets() */

/*--------------------------------------------------------------*/
/* create_obstructions_from_variable_pitch()			*/
/*								*/
/*  Although it would be nice to have an algorithm that would	*/
/*  work with any arbitrary pitch, qrouter will work around	*/
/*  having larger pitches on upper metal layers by selecting	*/
/*  1 out of every N tracks for routing, and placing 		*/
/*  obstructions in the interstices.  This makes the possibly	*/
/*  unwarranted assumption that the contact down to the layer	*/
/*  below does not cause spacing violations to neighboring	*/
/*  tracks.  If that assumption fails, this routine will have	*/
/*  to be revisited.						*/
/*--------------------------------------------------------------*/

void create_obstructions_from_variable_pitch()
{
   int l, o, vnum, hnum, x, y;
   double vpitch, hpitch;

   for (l = 0; l < Num_layers; l++) {
      o = LefGetRouteOrientation(l);
      if (o == 1) {	// Horizontal route
	 vpitch = LefGetRoutePitch(l);
	 hpitch = LefGetRouteWidth(l) + LefGetRouteSpacing(l);
      }
      else {		// Vertical route
	 hpitch = LefGetRoutePitch(l);
	 vpitch = LefGetRouteWidth(l) + LefGetRouteSpacing(l);
      }

      vnum = 1;
      while (vpitch > PitchY[l]) {
	 vpitch /= 2.0;
	 vnum++;
      }
      hnum = 1;
      while (hpitch > PitchX[l]) {
	 hpitch /= 2.0;
	 hnum++;
      }
      if (vnum > 1 || hnum > 1) {
	 for (x = 0; x < NumChannelsX[l]; x++) {
	    if (x % hnum == 0) continue;
	    for (y = 0; y < NumChannelsY[l]; y++) {
	       if (y % vnum == 0) continue;
	       Obs[l][OGRID(x, y, l)] = NO_NET;
	    }
	 }
      }
   }
}

/*--------------------------------------------------------------*/
/* disable_gridpos() ---					*/
/*	Render the position at (x, y, lay) unroutable by	*/
/*	setting its Obs[] entry to NO_NET and removing it from	*/
/*	the Nodeloc and Nodesav records.			*/
/*--------------------------------------------------------------*/

void
disable_gridpos(int x, int y, int lay)
{
   int apos = OGRID(x, y, lay);

   Obs[lay][apos] = (u_int)(NO_NET | OBSTRUCT_MASK);
   Nodeloc[lay][apos] = NULL;
   Nodesav[lay][apos] = NULL;
   Stub[lay][apos] = 0.0;
}

/*--------------------------------------------------------------*/
/* check_obstruct()---						*/
/*	Called from create_obstructions_from_gates(), this	*/
/* 	routine takes a grid point at (gridx, gridy) (physical	*/
/* 	position (dx, dy)) and an obstruction defined by the	*/
/*	rectangle "ds", and sets flags and fills the Obsinfo	*/
/*	array to reflect how the obstruction affects routing to	*/
/*	the grid position.					*/
/*--------------------------------------------------------------*/

void
check_obstruct(int gridx, int gridy, DSEG ds, double dx, double dy)
{
    int *obsptr;
    float dist;

    obsptr = &(Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]);
    dist = Obsinfo[ds->layer][OGRID(gridx, gridy, ds->layer)];

    // Grid point is inside obstruction + halo.
    *obsptr |= NO_NET;

    // Completely inside obstruction?
    if (dy > ds->y1 && dy < ds->y2 && dx > ds->x1 && dx < ds->x2)
       *obsptr |= OBSTRUCT_MASK;

    else {

       // Make more detailed checks in each direction

       if (dy < ds->y1) {
	  if ((*obsptr & (OBSTRUCT_MASK & ~OBSTRUCT_N)) == 0) {
	     if ((dist == 0) || ((ds->y1 - dy) < dist))
		Obsinfo[ds->layer][OGRID(gridx, gridy, ds->layer)] = ds->y1 - dy;
	     *obsptr |= OBSTRUCT_N;
	  }
	  else *obsptr |= OBSTRUCT_MASK;
       }
       else if (dy > ds->y2) {
	  if ((*obsptr & (OBSTRUCT_MASK & ~OBSTRUCT_S)) == 0) {
	     if ((dist == 0) || ((dy - ds->y2) < dist))
		Obsinfo[ds->layer][OGRID(gridx, gridy, ds->layer)] = dy - ds->y2;
	     *obsptr |= OBSTRUCT_S;
	  }
	  else *obsptr |= OBSTRUCT_MASK;
       }
       if (dx < ds->x1) {
	  if ((*obsptr & (OBSTRUCT_MASK & ~OBSTRUCT_E)) == 0) {
	     if ((dist == 0) || ((ds->x1 - dx) < dist))
		Obsinfo[ds->layer][OGRID(gridx, gridy, ds->layer)] = ds->x1 - dx;
             *obsptr |= OBSTRUCT_E;
	  }
	  else *obsptr |= OBSTRUCT_MASK;
       }
       else if (dx > ds->x2) {
	  if ((*obsptr & (OBSTRUCT_MASK & ~OBSTRUCT_W)) == 0) {
	     if ((dist == 0) || ((dx - ds->x2) < dist))
		Obsinfo[ds->layer][OGRID(gridx, gridy, ds->layer)] = dx - ds->x2;
	     *obsptr |= OBSTRUCT_W;
	  }
	  else *obsptr |= OBSTRUCT_MASK;
       }
   }
}

/*--------------------------------------------------------------*/
/* Find the amount of clearance needed between an obstruction	*/
/* and a route track position.  This takes into consideration	*/
/* whether the obstruction is wide or narrow metal, if the	*/
/* spacing rules are graded according to metal width, and if a	*/
/* via placed at the position is or is not symmetric in X and Y	*/
/*--------------------------------------------------------------*/

double get_clear(int lay, int horiz, DSEG rect) {
   double vdelta, v2delta, mdelta, mwidth;

   vdelta = LefGetViaWidth(lay, lay, 1 - horiz);
   if (lay > 0) {
	v2delta = LefGetViaWidth(lay - 1, lay, 1 - horiz);
	if (v2delta > vdelta) vdelta = v2delta;
   }
   vdelta = vdelta / 2.0;

   // Spacing rule is determined by the minimum metal width,
   // either in X or Y, regardless of the position of the
   // metal being checked.

   mwidth = MIN(rect->x2 - rect->x1, rect->y2 - rect->y1);
   mdelta = LefGetRouteWideSpacing(lay, mwidth);

   return vdelta + mdelta;
}

/*--------------------------------------------------------------*/
/* Find the distance from a point to the edge of a clearance	*/
/* around a rectangle.  To satisfy euclidean distance rules, 	*/
/* the clearance is clrx on the left and right sides of the	*/
/* rectangle, clry on the top and bottom sides, and rounded on	*/
/* the corners.							*/
/*								*/
/* Return 1 if point passes clearance test, 0 if not.		*/
/*								*/
/* This routine not currently being used, but probably will	*/
/* need to be, eventually, to get a correct evaluation of	*/
/* tightly-spaced taps that violate manhattan rules but pass	*/
/* euclidean rules.						*/
/*--------------------------------------------------------------*/

char point_clearance_to_rect(double dx, double dy, DSEG ds,
	double clrx, double clry)
{
   double delx, dely, dist, xp, yp, alpha, yab;
   struct dseg_ dexp;

   dexp.x1 = ds->x1 - clrx;
   dexp.x2 = ds->x2 + clrx;
   dexp.y1 = ds->y1 - clry;
   dexp.y2 = ds->y2 + clry;

   /* If the point is between ds top and bottom, distance is	*/
   /* simple.							*/

   if (dy <= ds->y2 && dy >= ds->y1) {
      if (dx < dexp.x1)
	return (dexp.x1 - dx) > 0 ? 1 : 0;
      else if (dx > dexp.x2)
	return (dx - dexp.x2) > 0 ? 1 : 0;
      else
	return 0;	// Point is inside rect
   }

   /* Likewise if the point is between ds right and left	*/

   if (dx <= ds->x2 && dx >= ds->x1) {
      if (dy < dexp.y1)
	return (dexp.y1 - dy) > 0 ? 1 : 0;
      else if (dy > dexp.y2)
	return (dy - dexp.y2) > 0 ? 1 : 0;
      else
	return 0;	// Point is inside rect
   }

   /* Treat corners individually */

   if (dy > ds->y2)
      yab = dy - ds->y2;
   else if (dy < ds->y1)
      yab = ds->y1 - dy;

   if (dx > ds->x2)
      delx = dx - ds->x2;
   else if (dx < ds->x1)
      delx = ds->x1 - dx;

   dely = yab * (clrx / clry);	// Normalize y clearance to x
   dist = delx * delx + dely * dely;
   return (dist > (clrx * clrx)) ? 1 : 0;
}

/*--------------------------------------------------------------*/
/* create_obstructions_from_gates()				*/
/*								*/
/*  Fills in the Obs[][] grid from obstructions that were	*/
/*  defined for each macro in the technology LEF file and	*/
/*  translated into a list of grid coordinates in each		*/
/*  instance.							*/
/*								*/
/*  Also, fills in the Obs[][] grid with obstructions that	*/
/*  are defined by nodes of the gate that are unconnected in	*/
/*  this netlist.						*/
/*--------------------------------------------------------------*/

void create_obstructions_from_gates()
{
    GATE g;
    DSEG ds;
    int i, gridx, gridy, *obsptr;
    double dx, dy, deltax, deltay, delta[MAX_LAYERS];
    float dist;

    // Give a single net number to all obstructions, over the range of the
    // number of known nets, so these positions cannot be routed through.
    // If a grid position is not wholly inside an obstruction, then we
    // maintain the direction of the nearest obstruction in Obs and the
    // distance to it in Obsinfo.  This indicates that a route can avoid
    // the obstruction by moving away from it by the amount in Obsinfo
    // plus spacing clearance.  If another obstruction is found that
    // prevents such a move, then all direction flags will be set, indicating
    // that the position is not routable under any condition. 

    for (g = Nlgates; g; g = g->next) {
       for (ds = g->obs; ds; ds = ds->next) {

	  deltax = get_clear(ds->layer, 1, ds);
	  gridx = (int)((ds->x1 - Xlowerbound - deltax)
			/ PitchX[ds->layer]) - 1;
	  while (1) {
	     dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
	     if ((dx + EPS) > (ds->x2 + deltax)
			|| gridx >= NumChannelsX[ds->layer]) break;
	     else if ((dx - EPS) > (ds->x1 - deltax) && gridx >= 0) {
		deltay = get_clear(ds->layer, 0, ds);
	        gridy = (int)((ds->y1 - Ylowerbound - deltay)
			/ PitchY[ds->layer]) - 1;
	        while (1) {
		   dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
	           if ((dy + EPS) > (ds->y2 + deltay)
				|| gridy >= NumChannelsY[ds->layer]) break;
		   if ((dy - EPS) > (ds->y1 - deltay) && gridy >= 0)
		      check_obstruct(gridx, gridy, ds, dx, dy);

		   gridy++;
		}
	     }
	     gridx++;
	  }
       }

       for (i = 0; i < g->nodes; i++) {
	  if (g->netnum[i] == 0) {	/* Unconnected node */
	     // Diagnostic, and power bus handling
	     if (g->node[i]) {
		if (vddnet && !strncmp(g->node[i], vddnet, strlen(vddnet)))
		   continue;
		else if (gndnet && !strncmp(g->node[i], gndnet, strlen(gndnet)))
		   continue;
		else
		   fprintf(stdout, "Gate instance %s unconnected node %s\n",
				g->gatename, g->node[i]);
	     }
	     else
	        fprintf(stdout, "Gate instance %s unconnected node (%d)\n",
			g->gatename, i);
             for (ds = g->taps[i]; ds; ds = ds->next) {

		deltax = get_clear(ds->layer, 1, ds);
		gridx = (int)((ds->x1 - Xlowerbound - deltax)
			/ PitchX[ds->layer]) - 1;
		while (1) {
		   dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		   if (dx > (ds->x2 + deltax)
				|| gridx >= NumChannelsX[ds->layer]) break;
		   else if (dx >= (ds->x1 - deltax) && gridx >= 0) {
		      deltay = get_clear(ds->layer, 0, ds);
		      gridy = (int)((ds->y1 - Ylowerbound - deltay)
				/ PitchY[ds->layer]) - 1;
		      while (1) {
		         dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		         if (dy > (ds->y2 + deltay)
					|| gridy >= NumChannelsY[ds->layer]) break;
		         if (dy >= (ds->y1 - deltay) && gridy >= 0)
			    check_obstruct(gridx, gridy, ds, dx, dy);

		         gridy++;
		      }
		   }
		   gridx++;
		}
	     }
	  }
       }
    }

    // Create additional obstructions from the UserObs list
    // These obstructions are not considered to be metal layers,
    // so we don't compute a distance measure.  However, we need
    // to compute a boundary of 1/2 route width to avoid having
    // the route overlapping the obstruction area.

    for (i = 0; i < Num_layers; i++) {
	delta[i] = LefGetRouteWidth(i) / 2.0;
    }

    for (ds = UserObs; ds; ds = ds->next) {
	gridx = (int)((ds->x1 - Xlowerbound - delta[ds->layer])
			/ PitchX[ds->layer]) - 1;
	while (1) {
	    dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
	    if (dx > (ds->x2 + delta[ds->layer])
			|| gridx >= NumChannelsX[ds->layer]) break;
	    else if (dx >= (ds->x1 - delta[ds->layer]) && gridx >= 0) {
		gridy = (int)((ds->y1 - Ylowerbound - delta[ds->layer])
				/ PitchY[ds->layer]) - 1;
		while (1) {
		    dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		    if (dy > (ds->y2 + delta[ds->layer])
				|| gridy >= NumChannelsY[ds->layer]) break;
		    if (dy >= (ds->y1 - delta[ds->layer]) && gridy >= 0) {
		        check_obstruct(gridx, gridy, ds, dx, dy);
		    }
		    gridy++;
		}
	    }
	    gridx++;
	}
    }
}

/*--------------------------------------------------------------*/
/* create_obstructions_from_nodes()				*/
/*								*/
/*  Fills in the Obs[][] grid from the position of each node	*/
/*  (net terminal), which may have multiple unconnected		*/
/*  positions.							*/
/*								*/
/*  Also fills in the Nodeloc[] grid with the node number,	*/
/*  which causes the router to put a premium on			*/
/*  routing other nets over or under this position, to		*/
/*  discourage boxing in a pin position and making it 		*/
/*  unroutable.							*/
/*								*/
/*  ARGS: none.							*/
/*  RETURNS: nothing						*/
/*  SIDE EFFECTS: none						*/
/*  AUTHOR:  Tim Edwards, June 2011, based on code by Steve	*/
/*	Beccue.							*/
/*--------------------------------------------------------------*/

void create_obstructions_from_nodes()
{
    NODE node, n2;
    GATE g;
    DPOINT dp;
    DSEG ds;
    u_int dir, k;
    int i, gx, gy, gridx, gridy, net;
    double dx, dy, deltax, deltay;
    float dist, xdist;
    double offmaxx[MAX_LAYERS], offmaxy[MAX_LAYERS];

    // Use a more conservative definition of keepout, to include via
    // widths, which may be bigger than route widths.

    for (i = 0; i < Num_layers; i++) {
	offmaxx[i] = PitchX[i] - LefGetRouteSpacing(i)
		- 0.5 * (LefGetRouteWidth(i) + LefGetViaWidth(i, i, 0));
	offmaxy[i] = PitchY[i] - LefGetRouteSpacing(i)
		- 0.5 * (LefGetRouteWidth(i) + LefGetViaWidth(i, i, 1));
    }

    // When we place vias at an offset, they have to satisfy the spacing
    // requirements for the via's top layer, as well.  So take the least
    // maximum offset of each layer and the layer above it.

    for (i = 0; i < Num_layers - 1; i++) {
       offmaxx[i] = MIN(offmaxx[i], offmaxx[i + 1]);
       offmaxy[i] = MIN(offmaxy[i], offmaxy[i + 1]);
    }

    // For each node terminal (gate pin), mark each grid position with the
    // net number.  This overrides any obstruction that may be placed at that
    // point.

    // For each pin position, we also find the "keepout" area around the
    // pin where we may not place an unrelated route.  For this, we use a
    // flag bit, so that the position can be ignored when routing the net
    // associated with the pin.  Normal obstructions take precedence.

    for (g = Nlgates; g; g = g->next) {
       for (i = 0; i < g->nodes; i++) {
	  if (g->netnum[i] != 0) {

	     // Get the node record associated with this pin.
	     node = g->noderec[i];

	     // First mark all areas inside node geometry boundary.

             for (ds = g->taps[i]; ds; ds = ds->next) {
		gridx = (int)((ds->x1 - Xlowerbound) / PitchX[ds->layer]) - 1;
		while (1) {
		   dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		   if (dx > ds->x2 || gridx >= NumChannelsX[ds->layer]) break;
		   else if (dx >= ds->x1 && gridx >= 0) {
		      gridy = (int)((ds->y1 - Ylowerbound) / PitchY[ds->layer]) - 1;
		      while (1) {
		         dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		         if (dy > ds->y2 || gridy >= NumChannelsY[ds->layer]) break;

			 // Area inside defined pin geometry

			 if (dy > ds->y1 && gridy >= 0) {
			     int orignet = Obs[ds->layer][OGRID(gridx,
					gridy, ds->layer)];

			     if ((orignet & ~PINOBSTRUCTMASK) == (u_int)node->netnum) {

				// Duplicate tap point.   Don't re-process it. (?)
				gridy++;
				continue;
			     }

			     if (!(orignet & NO_NET) &&
					((orignet & ~PINOBSTRUCTMASK) != (u_int)0)) {

				// Net was assigned to other net, but is inside
				// this pin's geometry.  Declare point to be
				// unroutable, as it is too close to both pins.
				// NOTE:  This is a conservative rule and could
				// potentially make a pin unroutable.
				// Another note:  By setting Obs[] to
				// OBSTRUCT_MASK as well as NO_NET, we ensure
				// that it falls through on all subsequent
				// processing.

				disable_gridpos(gridx, gridy, ds->layer);
			     }
			     else if (!(orignet & NO_NET)) {

				// A grid point that is within 1/2 route width
				// of a tap rectangle corner can violate metal
				// width rules, and so should declare a stub.
				
				dir = 0;
				dist = 0.0;
			        xdist = 0.5 * LefGetRouteWidth(ds->layer);

				if (dx >= ds->x2 - xdist) {
				   if (dy >= ds->y2 - xdist) {
				      // Check northeast corner

				      if ((ds->x2 - dx) > (ds->y2 - dy)) {
					 // West-pointing stub
					 dir = STUBROUTE_EW;
					 dist = ds->x2 - dx - 2.0 * xdist;
				      }
				      else {
					 // South-pointing stub
					 dir = STUBROUTE_NS;
					 dist = ds->y2 - dy - 2.0 * xdist;
				      }

				   }
				   else if (dy <= ds->y1 + xdist) {
				      // Check southeast corner

				      if ((ds->x2 - dx) > (dy - ds->y1)) {
					 // West-pointing stub
					 dir = STUBROUTE_EW;
					 dist = ds->x2 - dx - 2.0 * xdist;
				      }
				      else {
					 // North-pointing stub
					 dir = STUBROUTE_NS;
					 dist = ds->y1 - dy + 2.0 * xdist;
				      }
				   }
				}
				else if (dx <= ds->x1 + xdist) {
				   if (dy >= ds->y2 - xdist) {
				      // Check northwest corner

				      if ((dx - ds->x1) > (ds->y2 - dy)) {
					 // East-pointing stub
					 dir = STUBROUTE_EW;
					 dist = ds->x1 - dx + 2.0 * xdist;
				      }
				      else {
					 // South-pointing stub
					 dir = STUBROUTE_NS;
					 dist = ds->y2 - dy - 2.0 * xdist;
				      }

				   }
				   else if (dy <= ds->y1 + xdist) {
				      // Check southwest corner

				      if ((dx - ds->x2) > (dy - ds->y1)) {
					 // East-pointing stub
					 dir = STUBROUTE_EW;
					 dist = ds->x1 - dx + 2.0 * xdist;
				      }
				      else {
					 // North-pointing stub
					 dir = STUBROUTE_NS;
					 dist = ds->y1 - dy + 2.0 * xdist;
				      }
				   }
				}

			        Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= (u_int)node->netnum | dir;
			        Nodeloc[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
			        Nodesav[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
			        Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= dist;

			     }
			     else if ((orignet & NO_NET) && ((orignet & OBSTRUCT_MASK)
					!= OBSTRUCT_MASK)) {
				double sdistx = LefGetViaWidth(ds->layer, ds->layer, 0)
					/ 2.0 + LefGetRouteSpacing(ds->layer);
				double sdisty = LefGetViaWidth(ds->layer, ds->layer, 1)
					/ 2.0 + LefGetRouteSpacing(ds->layer);
				double offd;

				// Define a maximum offset we can have in X or
				// Y above which the placement of a via will
				// cause a DRC violation with a wire in the
				// adjacent route track in the direction of the
				// offset.

				int maxerr = 0;

				// If a cell is positioned off-grid, then a grid
				// point may be inside a pin and still be unroutable.
				// The Obsinfo[] array tells where an obstruction is,
				// if there was only one obstruction in one direction
				// blocking the grid point.  If so, then we set the
				// Stub[] distance to move the tap away from the
				// obstruction to resolve the DRC error.

				// Make sure we have marked this as a node.
			        Nodeloc[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
			        Nodesav[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
			        Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= (u_int)node->netnum;

				if (orignet & OBSTRUCT_N) {
			           offd = -(sdisty - Obsinfo[ds->layer]
					[OGRID(gridx, gridy, ds->layer)]);
				   if (offd >= -offmaxy[ds->layer]) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= offd;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= (STUBROUTE_NS | OFFSET_TAP);
				   }
				   else maxerr = 1;
				}
				else if (orignet & OBSTRUCT_S) {
				   offd = sdisty - Obsinfo[ds->layer]
					[OGRID(gridx, gridy, ds->layer)];
				   if (offd <= offmaxy[ds->layer]) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= offd;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= (STUBROUTE_NS | OFFSET_TAP);
				   }
				   else maxerr = 1;
				}
				else if (orignet & OBSTRUCT_E) {
				   offd = -(sdistx - Obsinfo[ds->layer]
					[OGRID(gridx, gridy, ds->layer)]);
				   if (offd >= -offmaxx[ds->layer]) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= offd;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= (STUBROUTE_EW | OFFSET_TAP);
				   }
				   else maxerr = 1;
				}
				else if (orignet & OBSTRUCT_W) {
				   offd = sdistx - Obsinfo[ds->layer]
					[OGRID(gridx, gridy, ds->layer)];
				   if (offd <= offmaxx[ds->layer]) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= offd;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= (STUBROUTE_EW | OFFSET_TAP);
				   }
				   else maxerr = 1;
				}

			        if (maxerr == 1)
				   disable_gridpos(gridx, gridy, ds->layer);

				// Diagnostic
				else if (Verbose > 0)
				   fprintf(stderr, "Port overlaps obstruction"
					" at grid %d %d, position %g %g\n",
					gridx, gridy, dx, dy);
			     }

			     // Check that we have not created a PINOBSTRUCT
			     // route directly over this point.
			     if (ds->layer < Num_layers - 1) {
			        k = Obs[ds->layer + 1][OGRID(gridx, gridy,
					ds->layer + 1)];
			        if (k & PINOBSTRUCTMASK) {
			           if ((k & ~PINOBSTRUCTMASK) != (u_int)node->netnum) {
				       Obs[ds->layer + 1][OGRID(gridx, gridy,
						ds->layer + 1)] = NO_NET;
				       Nodeloc[ds->layer + 1][OGRID(gridx, gridy,
						ds->layer + 1)] = (NODE)NULL;
				       Nodesav[ds->layer + 1][OGRID(gridx, gridy,
						ds->layer + 1)] = (NODE)NULL;
				       Stub[ds->layer + 1][OGRID(gridx, gridy,
						ds->layer + 1)] = (float)0.0;
				   }
				}
			     }
			 }
		         gridy++;
		      }
		   }
		   gridx++;
		}
	     }

	     // Repeat this whole exercise for areas in the halo outside
	     // the node geometry.  We have to do this after enumerating
	     // all inside areas because the tap rectangles often overlap,
	     // and one rectangle's halo may be inside another tap.

             for (ds = g->taps[i]; ds; ds = ds->next) {
		deltax = get_clear(ds->layer, 1, ds);
		gridx = (int)((ds->x1 - Xlowerbound - deltax)
			/ PitchX[ds->layer]) - 1;

		while (1) {
		   dx = (gridx * PitchX[ds->layer]) + Xlowerbound;

		   if ((dx + EPS) > (ds->x2 + deltax) ||
				gridx >= NumChannelsX[ds->layer])
		      break;

		   else if ((dx - EPS) > (ds->x1 - deltax) && gridx >= 0) {
		      deltay = get_clear(ds->layer, 0, ds);
		      gridy = (int)((ds->y1 - Ylowerbound - deltay)
				/ PitchY[ds->layer]) - 1;

		      while (1) {
		         dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		         if ((dy + EPS) > (ds->y2 + deltay) ||
				gridy >= NumChannelsY[ds->layer])
			    break;

		         if ((dy - EPS) > (ds->y1 - deltay) && gridy >= 0) {
			    xdist = 0.5 * LefGetRouteWidth(ds->layer);

			    // Area inside halo around defined pin geometry.
			    // Exclude areas already processed (areas inside
			    // some pin geometry have been marked with netnum)

			    // Also check that we are not about to define a
			    // route position for a pin on a layer above 0 that
			    // blocks a pin underneath it.

			    n2 = NULL;
			    if (ds->layer > 0)
			       n2 = Nodeloc[ds->layer - 1][OGRID(gridx, gridy,
					ds->layer - 1)];
			    if (n2 == NULL)
			       n2 = Nodeloc[ds->layer][OGRID(gridx, gridy, ds->layer)];

			    // Ignore my own node.
			    if (n2 == node) n2 = NULL;

			    k = Obs[ds->layer][OGRID(gridx, gridy, ds->layer)];

			    // In case of a port that is inaccessible from a grid
			    // point, or not completely overlapping it, the
			    // stub information will show how to adjust the
			    // route position to cleanly attach to the port.

			    dir = STUBROUTE_X;
			    dist = 0.0;

			    if (((k & ~PINOBSTRUCTMASK) != (u_int)node->netnum) &&
					(n2 == NULL)) {

				if ((k & OBSTRUCT_MASK) != 0) {
				   float sdist = Obsinfo[ds->layer][OGRID(gridx,
						gridy, ds->layer)];

				   // If the point is marked as close to an
				   // obstruction, we can declare this an
				   // offset tap if we are not on a corner.
				   // Because we cannot define both an offset
				   // and a stub simultaneously, if the distance
				   // to clear the obstruction does not make the
				   // route reach the tap, then we mark the grid
				   // position as unroutable.

				   if (dy >= (ds->y1 - xdist) &&
						dy <= (ds->y2 + xdist)) {
				      if ((dx >= ds->x2) &&
						((k & OBSTRUCT_MASK) == OBSTRUCT_E)) {
				         dist = sdist - LefGetRouteKeepout(ds->layer);
					 if ((dx - ds->x2 + dist) < xdist)
				 	    dir = STUBROUTE_EW | OFFSET_TAP;
				      }
				      else if ((dx <= ds->x1) &&
						((k & OBSTRUCT_MASK) == OBSTRUCT_W)) {
				         dist = LefGetRouteKeepout(ds->layer) - sdist;
					 if ((ds->x1 - dx - dist) < xdist)
				            dir = STUBROUTE_EW | OFFSET_TAP;
				      }
			 	   }	
				   if (dx >= (ds->x1 - xdist) &&
						dx <= (ds->x2 + xdist)) {
				      if ((dy >= ds->y2) &&
						((k & OBSTRUCT_MASK) == OBSTRUCT_N)) {
				         dist = sdist - LefGetRouteKeepout(ds->layer);
					 if ((dy - ds->y2 + dist) < xdist)
				            dir = STUBROUTE_NS | OFFSET_TAP;
				      }
				      else if ((dy <= ds->y1) &&
						((k & OBSTRUCT_MASK) == OBSTRUCT_S)) {
				         dist = LefGetRouteKeepout(ds->layer) - sdist;
					 if ((ds->y1 - dy - dist) < xdist)
				            dir = STUBROUTE_NS | OFFSET_TAP;
				      }
				   }
				   // Otherwise, dir is left as STUBROUTE_X
				}
				else {

				   // Cleanly unobstructed area.  Define stub
				   // route from point to tap, with a route width
				   // overlap if necessary to avoid a DRC width
				   // violation.

				   if ((dx >= ds->x2) &&
					((dx - ds->x2) > (dy - ds->y2)) &&
					((dx - ds->x2) > (ds->y1 - dy))) {
				      // West-pointing stub
				      if ((dy - ds->y2) <= xdist &&
					  (ds->y1 - dy) <= xdist) {
					 // Within reach of tap rectangle
					 dir = STUBROUTE_EW;
					 dist = ds->x2 - dx;
					 if (dy < (ds->y2 - xdist) &&
						dy > (ds->y1 + xdist)) {
					    if (dx < ds->x2 + xdist) dist = 0.0;
					 }
					 else {
					    dist -= 2.0 * xdist;
					 }
				      }
				   }
				   else if ((dx <= ds->x1) &&
					((ds->x1 - dx) > (dy - ds->y2)) &&
					((ds->x1 - dx) > (ds->y1 - dy))) {
				      // East-pointing stub
				      if ((dy - ds->y2) <= xdist &&
					  (ds->y1 - dy) <= xdist) {
					 // Within reach of tap rectangle
					 dir = STUBROUTE_EW;
					 dist = ds->x1 - dx;
					 if (dy < (ds->y2 - xdist) &&
						dy > (ds->y1 + xdist)) {
					    if (dx > ds->x1 - xdist) dist = 0.0;
					 }
					 else {
					    dist += 2.0 * xdist;
					 }
				      }
				   }
				   else if ((dy >= ds->y2) &&
					((dy - ds->y2) > (dx - ds->x2)) &&
					((dy - ds->y2) > (ds->x1 - dx))) {
				      // South-pointing stub
				      if ((dx - ds->x2) <= xdist &&
					  (ds->x1 - dx) <= xdist) {
					 // Within reach of tap rectangle
					 dir = STUBROUTE_NS;
					 dist = ds->y2 - dy;
					 if (dx < (ds->x2 - xdist) &&
						dx > (ds->x1 + xdist)) {
					    if (dy < ds->y2 + xdist) dist = 0.0;
					 }
					 else {
					    dist -= 2.0 * xdist;
					 }
				      }
				   }
				   else if ((dy <= ds->y1) &&
					((ds->y1 - dy) > (dx - ds->x2)) &&
					((ds->y1 - dy) > (ds->x1 - dx))) {
				      // North-pointing stub
				      if ((dx - ds->x2) <= xdist &&
					  (ds->x1 - dx) <= xdist) {
					 // Within reach of tap rectangle
					 dir = STUBROUTE_NS;
					 dist = ds->y1 - dy;
					 if (dx < (ds->x2 - xdist) &&
						dx > (ds->x1 + xdist)) {
					    if (dy > ds->y1 - xdist) dist = 0.0;
					 }
					 else {
					    dist += 2.0 * xdist;
					 }
				      }
				   }

				   if (dir == STUBROUTE_X) {

				      // Outside of pin at a corner.  First, if one
				      // direction is too far away to connect to a
				      // pin, then we must route the other direction.

				      if (dx < ds->x1 - xdist || dx > ds->x2 + xdist) {
				         if (dy >= ds->y1 - xdist &&
							dy <= ds->y2 + xdist) {
				            dir = STUBROUTE_EW;
				            dist = (float)(((ds->x1 + ds->x2) / 2.0)
							- dx);
					 }
				      }
				      else if (dy < ds->y1 - xdist ||
							dy > ds->y2 + xdist) {
				         dir = STUBROUTE_NS;
				         dist = (float)(((ds->y1 + ds->y2) / 2.0) - dy);
				      }

				      // Otherwise we are too far away at a diagonal
				      // to reach the pin by moving in any single
				      // direction.  To be pedantic, we could define
				      // some jogged stub, but for now, we just call
				      // the point unroutable (leave dir = STUBROUTE_X)
				   }
				}

				// Stub distances of <= 1/2 route width are useless
				if (dir == STUBROUTE_NS || dir == STUBROUTE_EW)
				   if (fabs(dist) < (xdist + EPS)) {
				      dir = 0;
				      dist = 0.0;
				   }

				if ((k < Numnets) && (dir != STUBROUTE_X)) {
				   Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= (u_int)g->netnum[i] | dir; 
				   Nodeloc[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
				   Nodesav[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= node;
				}
				else {
				   // Keep showing an obstruction, but add the
				   // direction info and log the stub distance.
				   Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
					|= dir;
				}
				Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
					= dist;
			    }
			    else {
			       int othernet = (k & ~PINOBSTRUCTMASK);

			       if (othernet != 0 && othernet != (u_int)node->netnum) {

			          // This location is too close to two different
				  // node terminals and should not be used
				  // (NOTE:  To be thorough, we should check
				  // if othernet could be routed using a tap
				  // offset.  By axing it we might lose the
				  // only route point to one of the pins.)
				  // (Another note:  This routine also disables
				  // catecorner positions that might pass
				  // euclidean distance DRC checks, so it's
				  // more restrictive than necessary.)
				
				  disable_gridpos(gridx, gridy, ds->layer);
			       }
			       else if (othernet == (u_int)node->netnum) {

				  // Check if a potential DRC violation can
				  // be removed by the addition of a stub route

				  xdist = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 0);
				  if ((dy + xdist + LefGetRouteSpacing(ds->layer) >
					ds->y1) && (dy + xdist < ds->y1)) {
				     if (Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->y1 - dy;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
				     }
				  }
				  if ((dy - xdist - LefGetRouteSpacing(ds->layer) <
					ds->y2) && (dy - xdist > ds->y2)) {
				     if (Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->y2 - dy;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
				     }
				  }

				  xdist = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 1);
				  if ((dx + xdist + LefGetRouteSpacing(ds->layer) >
					ds->x1) && (dx + xdist < ds->x1)) {
				     if (Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->x1 - dx;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
				     }
				  }
				  if ((dx - xdist - LefGetRouteSpacing(ds->layer) <
					ds->x2) && (dx - xdist > ds->x2)) {
				     if (Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->x2 - dx;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
				     }
				  }
			       }

			       /* If we are on a layer > 0, then this geometry	*/
			       /* may block or partially block a pin on layer	*/
			       /* zero.  Mark this point as belonging to the	*/
			       /* net with a stub route to it.			*/
			       /* NOTE:  This is possibly too restrictive.	*/
			       /* May want to force a tap offset for vias on	*/
			       /* layer zero. . .				*/

			       if ((ds->layer > 0) && (n2 != NULL) && (n2->netnum
					!= node->netnum) && ((othernet == 0) ||
					(othernet == (u_int)node->netnum))) {

				  xdist = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 0);
				  if ((dy + xdist + LefGetRouteSpacing(ds->layer) >
					ds->y1) && (dy + xdist < ds->y1)) {
				     if ((dx - xdist < ds->x2) &&
						(dx + xdist > ds->x1) &&
						(Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0)) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->y1 - dy;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= node->netnum | STUBROUTE_NS;
					Nodeloc[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
					Nodesav[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
				     }
				  }
				  if ((dy - xdist - LefGetRouteSpacing(ds->layer) <
					ds->y2) && (dy - xdist > ds->y2)) {
				     if ((dx - xdist < ds->x2) &&
						(dx + xdist > ds->x1) &&
						(Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0)) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->y2 - dy;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= node->netnum | STUBROUTE_NS;
					Nodeloc[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
					Nodesav[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
				     }
				  }

				  xdist = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 1);
				  if ((dx + xdist + LefGetRouteSpacing(ds->layer) >
					ds->x1) && (dx + xdist < ds->x1)) {
				     if ((dy - xdist < ds->y2) &&
						(dy + xdist > ds->y1) &&
						(Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0)) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->x1 - dx;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= node->netnum | STUBROUTE_EW;
					Nodeloc[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
					Nodesav[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
				     }
				  }
				  if ((dx - xdist - LefGetRouteSpacing(ds->layer) <
					ds->x2) && (dx - xdist > ds->x2)) {
				     if ((dy - xdist < ds->y2) &&
						(dy + xdist > ds->y1) &&
						(Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] == 0.0)) {
					Stub[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = ds->x2 - dx;
					Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= node->netnum | STUBROUTE_EW;
					Nodeloc[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
					Nodesav[ds->layer][OGRID(gridx, gridy,
						ds->layer)] = node;
				     }
				  }
			       }
			    }
		         }
		         gridy++;
		      }
		   }
		   gridx++;
		}
	     }
	  }
       }
    }

} /* void create_obstructions_from_nodes( void ) */

/*--------------------------------------------------------------*/
/* tap_to_tap_interactions()					*/
/*								*/
/*  Similar to create_obstructions_from_nodes(), but looks at	*/
/*  each node's tap geometry, looks at every grid point in a	*/
/*  wider area surrounding the tap.  If any other node has an	*/
/*  offset that would place it too close to this node's	tap	*/
/*  geometry, then we mark the other node as unroutable at that	*/
/*  grid point.							*/
/*--------------------------------------------------------------*/

void tap_to_tap_interactions()
{
    NODE node;
    GATE g;
    DSEG ds;
    struct dseg_ de;
    int mingridx, mingridy, maxgridx, maxgridy;
    int i, gridx, gridy, net, orignet, offset;
    double dx, dy;
    float dist;
    u_char errbox;

    double delta[MAX_LAYERS];

    for (i = 0; i < Num_layers; i++) {
	delta[i] = 0.5 * LefGetViaWidth(i, i, 0) + LefGetRouteSpacing(i);
	// NOTE:  Extra space is how much vias get shifted relative to the
	// specified offset distance to account for the via size being larger
	// than the route width.
	delta[i] += 0.5 * (LefGetViaWidth(i, i, 0) - LefGetRouteSpacing(i));
    }

    for (g = Nlgates; g; g = g->next) {
       for (i = 0; i < g->nodes; i++) {
	  net = g->netnum[i];
	  if (net != 0) {

	     // Get the node record associated with this pin.
	     node = g->noderec[i];

             for (ds = g->taps[i]; ds; ds = ds->next) {

		mingridx = (int)((ds->x1 - Xlowerbound) / PitchX[ds->layer]) - 1;
		if (mingridx < 0) mingridx = 0;
		maxgridx = (int)((ds->x2 - Xlowerbound) / PitchX[ds->layer]) + 2;
		if (maxgridx >= NumChannelsX[ds->layer])
		   maxgridx = NumChannelsX[ds->layer] - 1;
		mingridy = (int)((ds->y1 - Ylowerbound) / PitchY[ds->layer]) - 1;
		if (mingridy < 0) mingridy = 0;
		maxgridy = (int)((ds->y2 - Ylowerbound) / PitchY[ds->layer]) + 2;
		if (maxgridy >= NumChannelsY[ds->layer])
		   maxgridy = NumChannelsY[ds->layer] - 1;

		for (gridx = mingridx; gridx <= maxgridx; gridx++) {
		   for (gridy = mingridy; gridy <= maxgridy; gridy++) {

		      /* Is there an offset tap at this position, and	*/
		      /* does it belong to a net that is != net?	*/

		      orignet = Obs[ds->layer][OGRID(gridx, gridy, ds->layer)];
		      if (orignet & OFFSET_TAP) {
			 offset = orignet & PINOBSTRUCTMASK;
			 orignet &= ~PINOBSTRUCTMASK;
			 if (orignet != net) {

		            dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		            dy = (gridy * PitchY[ds->layer]) + Ylowerbound;

			    dist = Stub[ds->layer][OGRID(gridx, gridy, ds->layer)];

			    /* "de" is the bounding box of a via placed	  */
			    /* at (gridx, gridy) and offset as specified. */
			    /* Expanded by metal spacing requirement.	  */

			    de.x1 = dx - delta[ds->layer];
			    de.x2 = dx + delta[ds->layer];
			    de.y1 = dy - delta[ds->layer];
			    de.y2 = dy + delta[ds->layer];

			    if (offset == (STUBROUTE_NS | OFFSET_TAP)) {
			       de.y1 += dist;
			       de.y2 += dist;
			    }
			    else if (offset == (STUBROUTE_EW | OFFSET_TAP)) {
			       de.x1 += dist;
			       de.x2 += dist;
			    }

			    /* Does the via bounding box interact with	*/
			    /* the tap geometry?			*/

			    if ((de.x1 < ds->x2) && (ds->x1 < de.x2) &&
					(de.y1 < ds->y2) && (ds->y1 < de.y2))
			       disable_gridpos(gridx, gridy, ds->layer);
			 }
		      }
		   }
		}
	     }
	  }
       }
    }
}

/*--------------------------------------------------------------*/
/* make_routable()						*/
/*								*/
/*  In the case that a node can't be routed because it has no	*/
/*  available tap points, but there is tap geometry recorded	*/
/*  for the node, then take the first available grid location	*/
/*  near the tap.  This, of course, bypasses all of qrouter's	*/
/*  DRC checks.  But it is only meant to be a stop-gap measure	*/
/*  to get qrouter to complete all routes, and may work in	*/
/*  cases where, say, the tap passes euclidean rules but not	*/
/*  manhattan rules.						*/
/*--------------------------------------------------------------*/

void
make_routable(NODE node)
{
    GATE g;
    DSEG ds;
    int i, gridx, gridy, net;
    double dx, dy;

    /* The database is not organized to find tap points	*/
    /* from nodes, so we have to search for the node.	*/
    /* Fortunately this routine isn't normally called.	*/

    for (g = Nlgates; g; g = g->next) {
       for (i = 0; i < g->nodes; i++) {
	  if (g->noderec[i] == node) {
             for (ds = g->taps[i]; ds; ds = ds->next) {
		gridx = (int)((ds->x1 - Xlowerbound) / PitchX[ds->layer]) - 1;
		while (1) {
		   dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		   if (dx > ds->x2 || gridx >= NumChannelsX[ds->layer]) break;
		   else if (dx >= ds->x1 && gridx >= 0) {
		      gridy = (int)((ds->y1 - Ylowerbound) / PitchY[ds->layer]) - 1;
		      while (1) {
		         dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		         if (dy > ds->y2 || gridy >= NumChannelsY[ds->layer]) break;

			 // Area inside defined pin geometry

			 if (dy > ds->y1 && gridy >= 0) {
			    int orignet = Obs[ds->layer][OGRID(gridx,
					gridy, ds->layer)];

			    if (orignet & NO_NET) {
				Obs[ds->layer][OGRID(gridx, gridy, ds->layer)] =
					g->netnum[i];
				Nodeloc[ds->layer][OGRID(gridx, gridy, ds->layer)] =
					node;
				Nodesav[ds->layer][OGRID(gridx, gridy, ds->layer)] =
					node;
				return;
			    }
			 }
			 gridy++;
		      }
		   }
		   gridx++;
	        }
	     }
	  }
       }
    }
}

/*--------------------------------------------------------------*/
/* adjust_stub_lengths()					*/
/*								*/
/*  Makes an additional pass through the tap and obstruction	*/
/*  databases, checking geometry against the potential stub	*/
/*  routes for DRC spacing violations.  Adjust stub routes as	*/
/*  necessary to resolve the DRC error(s).			*/
/*								*/
/*  ARGS: none.							*/
/*  RETURNS: nothing						*/
/*  SIDE EFFECTS: none						*/
/*  AUTHOR:  Tim Edwards, April 2013				*/
/*--------------------------------------------------------------*/

void adjust_stub_lengths()
{
    NODE node, n2;
    GATE g;
    DPOINT dp;
    DSEG ds, ds2;
    struct dseg_ dt, de;
    u_int dir, k;
    int i, gx, gy, gridx, gridy, net, orignet;
    double dx, dy, w, s, dd;
    float dist;
    u_char errbox;

    // For each node terminal (gate pin), look at the surrounding grid points.
    // If any define a stub route or an offset, check if the stub geometry
    // or offset geometry would create a DRC spacing violation.  If so, adjust
    // the stub route to resolve the error.  If the error cannot be resolved,
    // mark the position as unroutable.  If it is the ONLY grid point accessible
    // to the pin, keep it as-is and flag a warning.

    // Unlike blockage-finding routines, which look in an area of a size equal
    // to the DRC interaction distance around a tap rectangle, this routine looks
    // out one grid pitch in each direction, to catch information about stubs that
    // may terminate within a DRC interaction distance of the tap rectangle.

    for (g = Nlgates; g; g = g->next) {
       for (i = 0; i < g->nodes; i++) {
	  if (g->netnum[i] != 0) {

	     // Get the node record associated with this pin.
	     node = g->noderec[i];

	     // Work through each rectangle in the tap geometry

             for (ds = g->taps[i]; ds; ds = ds->next) {
		// w = 0.5 * LefGetRouteWidth(ds->layer);
		w = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 0);
		s = LefGetRouteSpacing(ds->layer);
		gridx = (int)((ds->x1 - Xlowerbound - PitchX[ds->layer])
			/ PitchX[ds->layer]) - 1;
		while (1) {
		   dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		   if (dx > (ds->x2 + PitchX[ds->layer]) ||
				gridx >= NumChannelsX[ds->layer]) break;
		   else if (dx >= (ds->x1 - PitchX[ds->layer]) && gridx >= 0) {
		      gridy = (int)((ds->y1 - Ylowerbound - PitchY[ds->layer])
				/ PitchY[ds->layer]) - 1;
		      while (1) {
		         dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		         if (dy > (ds->y2 + PitchY[ds->layer]) ||
				gridy >= NumChannelsY[ds->layer]) break;
		         if (dy >= (ds->y1 - PitchY[ds->layer]) && gridy >= 0) {

			     orignet = Obs[ds->layer][OGRID(gridx, gridy, ds->layer)];

			     // Ignore this location if it is assigned to another
			     // net, or is assigned to NO_NET.

			     if ((orignet & ~PINOBSTRUCTMASK) != node->netnum) {
				gridy++;
				continue;
			     }

			     // STUBROUTE_X are unroutable;  leave them alone
			     if ((orignet & PINOBSTRUCTMASK) == STUBROUTE_X) {
				gridy++;
				continue;
			     }

			     // define a route box around the grid point

			     errbox = FALSE;
			     dt.x1 = dx - w;
			     dt.x2 = dx + w;
			     dt.y1 = dy - w;
			     dt.y2 = dy + w;

			     dist = Stub[ds->layer][OGRID(gridx, gridy, ds->layer)];

			     // adjust the route box according to the stub
			     // or offset geometry

			     if (orignet & OFFSET_TAP) {
				if (orignet & STUBROUTE_EW) {
				   dt.x1 += dist;
				   dt.x2 += dist;
				}
				else if (orignet & STUBROUTE_NS) {
				   dt.y1 += dist;
				   dt.y2 += dist;
				}
			     }
			     else if (orignet & PINOBSTRUCTMASK) {
				if (orignet & STUBROUTE_EW) {
				   if (dist > EPS)
				      dt.x2 = dx + dist;
				   else
				      dt.x1 = dx + dist;
				}
				else if (orignet & STUBROUTE_NS) {
				   if (dist > EPS)
				      dt.y2 = dy + dist;
				   else
				      dt.y1 = dy + dist;
				}
			     }

			     de = dt;

			     // check for DRC spacing interactions between
			     // the tap box and the route box

			     if ((dt.y1 - ds->y2) > EPS && (dt.y1 - ds->y2) < s) {
				if (ds->x2 > (dt.x1 - s) && ds->x1 < (dt.x2 + s)) {
				   de.y2 = dt.y1;
				   de.y1 = ds->y2;
				   errbox = TRUE;
				}
			     }
			     else if ((ds->y1 - dt.y2) > EPS && (ds->y1 - dt.y2) < s) {
				if (ds->x2 > (dt.x1 - s) && ds->x1 < (dt.x2 + s)) {
				   de.y1 = dt.y2;
				   de.y2 = ds->y1;
				   errbox = TRUE;
				}
			     }

			     if ((dt.x1 - ds->x2) > EPS && (dt.x1 - ds->x2) < s) {
				if (ds->y2 > (dt.y1 - s) && ds->y1 < (dt.y2 + s)) {
				   de.x2 = dt.x1;
				   de.x1 = ds->x2;
				   errbox = TRUE;
				}
			     }
			     else if ((ds->x1 - dt.x2) > EPS && (ds->x1 - dt.x2) < s) {
				if (ds->y2 > (dt.y1 - s) && ds->y1 < (dt.y2 + s)) {
				   de.x1 = dt.x2;
				   de.x2 = ds->x1;
				   errbox = TRUE;
				}
			     }

			     if (errbox == TRUE) {
	
			        // Chop areas off the error box that are covered by
			        // other taps of the same port.

			        for (ds2 = g->taps[i]; ds2; ds2 = ds2->next) {
				   if (ds2 == ds) continue;
				   if (ds2->layer != ds->layer) continue;

				   if (ds2->x1 <= de.x1 && ds2->x2 >= de.x2 &&
					ds2->y1 <= de.y1 && ds2->y2 >= de.y2) {
				      errbox = FALSE;	// Completely covered
				      break;
				   }

				   // Look for partial coverage.  Note that any
				   // change can cause a change in the original
				   // two conditionals, so we have to keep
				   // evaluating those conditionals.

				   if (ds2->x1 < de.x2 && ds2->x2 > de.x1)
				      if (ds2->y1 < de.y2 && ds2->y2 > de.y1)
					 if (ds2->x1 < de.x1 && ds2->x2 < de.x2)
					    de.x1 = ds2->x2;

				   if (ds2->x1 < de.x2 && ds2->x2 > de.x1)
				      if (ds2->y1 < de.y2 && ds2->y2 > de.y1)
					 if (ds2->x2 > de.x2 && ds2->x1 > de.x1)
					    de.x2 = ds2->x1;

				   if (ds2->x1 < de.x2 && ds2->x2 > de.x1)
				      if (ds2->y1 < de.y2 && ds2->y2 > de.y1)
					 if (ds2->y1 < de.y1 && ds2->y2 < de.y2)
					    de.y1 = ds2->y2;

				   if (ds2->x1 < de.x2 && ds2->x2 > de.x1)
				      if (ds2->y1 < de.y2 && ds2->y2 > de.y1)
					 if (ds2->y2 > de.y2 && ds2->y1 > de.y1)
					    de.y2 = ds2->y1;
				}
			     }

			     // Any area left over is a potential DRC error.

			     if ((de.x2 <= de.x1) || (de.y2 <= de.y1))
				errbox = FALSE;
		
			     if (errbox == TRUE) {

				// Create stub route to cover error box, or
				// if possible, stretch existing stub route
				// to cover error box.

				// Allow EW stubs to be changed to NS stubs and
				// vice versa if the original stub length was less
				// than a route width.  This means the grid position
				// makes contact without the stub.  Moving the stub
				// to another side should not create an error.

				// NOTE:  Changed 4/29/13;  direction of stub will
				// be changed even though it might create an error
				// in the other direction;  it can't do worse.
				// But, the case should be re-run to check (to-do)

				if (de.x2 > dt.x2) {
				   if ((orignet & PINOBSTRUCTMASK) == 0) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x2 - dx;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) == STUBROUTE_EW
						&& (dist > 0)) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x2 - dx;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) ==
						STUBROUTE_NS) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						&= ~STUBROUTE_NS;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x2 - dx;
				      errbox = FALSE;
				   }
				}
				else if (de.x1 < dt.x1) {
				   if ((orignet & PINOBSTRUCTMASK) == 0) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x1 - dx;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) == STUBROUTE_EW
						&& (dist < 0)) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x1 - dx;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) ==
						STUBROUTE_NS) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						&= ~STUBROUTE_NS;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_EW;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.x1 - dx;
				      errbox = FALSE;
				   }
				}
				else if (de.y2 > dt.y2) {
				   if ((orignet & PINOBSTRUCTMASK) == 0) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y2 - dy;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) == STUBROUTE_NS
						&& (dist > 0)) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y2 - dy;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) ==
						STUBROUTE_EW) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						&= ~STUBROUTE_EW;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y2 - dy;
				      errbox = FALSE;
				   }
				}
				else if (de.y1 < dt.y1) {
				   if ((orignet & PINOBSTRUCTMASK) == 0) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y1 - dy;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) == STUBROUTE_NS
						&& (dist < 0)) {
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y1 - dy;
				      errbox = FALSE;
				   }
				   else if ((orignet & PINOBSTRUCTMASK) ==
						STUBROUTE_EW) {
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						&= ~STUBROUTE_EW;
			              Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
						|= STUBROUTE_NS;
			              Stub[ds->layer][OGRID(gridx, gridy, ds->layer)]
						= de.y1 - dy;
				      errbox = FALSE;
				   }
				}

				if (errbox == TRUE) {
				   // Unroutable position, so mark it unroutable
			           Obs[ds->layer][OGRID(gridx, gridy, ds->layer)]
					|= STUBROUTE_X;
				}
			     }
		         }
		         gridy++;
		      }
		   }
		   gridx++;
		}
	     }
	  }
       }
    }

} /* void adjust_stub_lengths() */

/*--------------------------------------------------------------*/
/* block_route()						*/
/*								*/
/*  Mark a specific length along the route tracks as unroutable	*/
/*  by finding the grid point in the direction indicated, and	*/
/*  setting the appropriate block bit in the Obs[] array for	*/
/*  that position.  The original grid point is marked as	*/
/*  unroutable in the opposite direction, for symmetry.		*/
/*--------------------------------------------------------------*/

void
block_route(int x, int y, int lay, u_char dir)
{
   int bx, by, ob;

   bx = x;
   by = y;

   switch (dir) {
      case NORTH:
	 if (y == NumChannelsY[lay] - 1) return;
	 by = y + 1;
	 break;
      case SOUTH:
	 if (y == 0) return;
	 by = y - 1;
	 break;
      case EAST:
	 if (x == NumChannelsX[lay] - 1) return;
	 bx = x + 1;
	 break;
      case WEST:
	 if (x == 0) return;
	 bx = x - 1;
	 break;
   }
   
   ob = Obs[lay][OGRID(bx, by, lay)];

   if ((ob & NO_NET) != 0) return;

   switch (dir) {
      case NORTH:
	 Obs[lay][OGRID(bx, by, lay)] |= BLOCKED_S;
	 Obs[lay][OGRID(x, y, lay)] |= BLOCKED_N;
	 break;
      case SOUTH:
	 Obs[lay][OGRID(bx, by, lay)] |= BLOCKED_N;
	 Obs[lay][OGRID(x, y, lay)] |= BLOCKED_S;
	 break;
      case EAST:
	 Obs[lay][OGRID(bx, by, lay)] |= BLOCKED_W;
	 Obs[lay][OGRID(x, y, lay)] |= BLOCKED_E;
	 break;
      case WEST:
	 Obs[lay][OGRID(bx, by, lay)] |= BLOCKED_E;
	 Obs[lay][OGRID(x, y, lay)] |= BLOCKED_W;
	 break;
   }
}

/*--------------------------------------------------------------*/
/* find_route_blocks() ---					*/
/*								*/
/*	Search tap geometry for edges that cause DRC spacing	*/
/*	errors with route edges.  This specifically checks	*/
/*	edges of the route tracks, not the intersection points.	*/
/*	If a tap would cause an error with a route segment,	*/
/*	the grid points on either end of the segment are	*/
/*	flagged to prevent generating a route along that	*/
/*	specific segment.					*/
/*--------------------------------------------------------------*/

void
find_route_blocks()
{
   NODE node;
   GATE g;
   // DPOINT dp;
   DSEG ds, ds2;
   struct dseg_ dt, de;
   int i, gridx, gridy;
   double dx, dy, w, v, s, u;
   float dist;
   u_char errbox;

   for (g = Nlgates; g; g = g->next) {
      for (i = 0; i < g->nodes; i++) {
	 if (g->netnum[i] != 0) {

	    // Get the node record associated with this pin.
	    node = g->noderec[i];

	    // Work through each rectangle in the tap geometry

            for (ds = g->taps[i]; ds; ds = ds->next) {
	       w = 0.5 * LefGetRouteWidth(ds->layer);
	       v = 0.5 * LefGetViaWidth(ds->layer, ds->layer, 0);
	       s = LefGetRouteSpacing(ds->layer);

	       // Look west

	       gridx = (int)((ds->x1 - Xlowerbound) / PitchX[ds->layer]);
	       dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
	       dist = ds->x1 - dx - w;
	       if (dist > 0 && dist < s && gridx >= 0) {
		  dt.x1 = dt.x2 = dx;
		  dt.y1 = ds->y1;
		  dt.y2 = ds->y2;

		  // Check for other taps covering this edge
		  // (to do)

		  // Find all grid points affected
	          gridy = (int)((ds->y1 - Ylowerbound - PitchY[ds->layer]) /
				PitchY[ds->layer]);
	          dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		  while (dy < ds->y1 - s) {
		     dy += PitchY[ds->layer];
		     gridy++;
		  }
		  while (dy < ds->y2 + s) {
		     u = ((Obs[ds->layer][OGRID(gridx, gridy, ds->layer)] &
				PINOBSTRUCTMASK) == STUBROUTE_EW) ? v : w;
		     if (dy + EPS < ds->y2 - u)
			block_route(gridx, gridy, ds->layer, NORTH);
		     if (dy - EPS > ds->y1 + u)
			block_route(gridx, gridy, ds->layer, SOUTH);
		     dy += PitchY[ds->layer];
		     gridy++;
		  }
	       }

	       // Look east

	       gridx = (int)(1.0 + (ds->x2 - Xlowerbound) / PitchX[ds->layer]);
	       dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
	       dist = dx - ds->x2 - w;
	       if (dist > 0 && dist < s && gridx < NumChannelsX[ds->layer]) {
		  dt.x1 = dt.x2 = dx;
		  dt.y1 = ds->y1;
		  dt.y2 = ds->y2;

		  // Check for other taps covering this edge
		  // (to do)

		  // Find all grid points affected
	          gridy = (int)((ds->y1 - Ylowerbound - PitchY[ds->layer]) /
				PitchY[ds->layer]);
	          dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
		  while (dy < ds->y1 - s) {
		     dy += PitchY[ds->layer];
		     gridy++;
		  }
		  while (dy < ds->y2 + s) {
		     u = ((Obs[ds->layer][OGRID(gridx, gridy, ds->layer)] &
				PINOBSTRUCTMASK) == STUBROUTE_EW) ? v : w;
		     if (dy + EPS < ds->y2 - u)
			block_route(gridx, gridy, ds->layer, NORTH);
		     if (dy - EPS > ds->y1 + u)
			block_route(gridx, gridy, ds->layer, SOUTH);
		     dy += PitchY[ds->layer];
		     gridy++;
		  }
	       }

	       // Look south

	       gridy = (int)((ds->y1 - Ylowerbound) / PitchY[ds->layer]);
	       dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
	       dist = ds->y1 - dy - w;
	       if (dist > 0 && dist < s && gridy >= 0) {
		  dt.x1 = ds->x1;
		  dt.x2 = ds->x2;
		  dt.y1 = dt.y2 = dy;

		  // Check for other taps covering this edge
		  // (to do)

		  // Find all grid points affected
	          gridx = (int)((ds->x1 - Xlowerbound - PitchX[ds->layer]) /
				PitchX[ds->layer]);
	          dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		  while (dx < ds->x1 - s) {
		     dx += PitchX[ds->layer];
		     gridx++;
		  }
		  while (dx < ds->x2 + s) {
		     u = ((Obs[ds->layer][OGRID(gridx, gridy, ds->layer)] &
				PINOBSTRUCTMASK) == STUBROUTE_NS) ? v : w;
		     if (dx + EPS < ds->x2 - u)
			block_route(gridx, gridy, ds->layer, EAST);
		     if (dx - EPS > ds->x1 + u)
			block_route(gridx, gridy, ds->layer, WEST);
		     dx += PitchX[ds->layer];
		     gridx++;
		  }
	       }

	       // Look north

	       gridy = (int)(1.0 + (ds->y2 - Ylowerbound) / PitchY[ds->layer]);
	       dy = (gridy * PitchY[ds->layer]) + Ylowerbound;
	       dist = dy - ds->y2 - w;
	       if (dist > 0 && dist < s && gridy < NumChannelsY[ds->layer]) {
		  dt.x1 = ds->x1;
		  dt.x2 = ds->x2;
		  dt.y1 = dt.y2 = dy;

		  // Check for other taps covering this edge
		  // (to do)

		  // Find all grid points affected
	          gridx = (int)((ds->x1 - Xlowerbound - PitchX[ds->layer]) /
				PitchX[ds->layer]);
	          dx = (gridx * PitchX[ds->layer]) + Xlowerbound;
		  while (dx < ds->x1 - s) {
		     dx += PitchX[ds->layer];
		     gridx++;
		  }
		  while (dx < ds->x2 + s) {
		     u = ((Obs[ds->layer][OGRID(gridx, gridy, ds->layer)] &
				PINOBSTRUCTMASK) == STUBROUTE_NS) ? v : w;
		     if (dx + EPS < ds->x2 - u)
			block_route(gridx, gridy, ds->layer, EAST);
		     if (dx - EPS > ds->x1 + u)
			block_route(gridx, gridy, ds->layer, WEST);
		     dx += PitchX[ds->layer];
		     gridx++;
		  }
	       }
	    }
	 }
      }
   }
}

/* node.c */
