#ifndef _CONSTFORBETACOMPLEX_H
#define _CONSTFORBETACOMPLEX_H

#include "rg_Const.h"
#include <string>
using namespace std;

const rg_INT ON_BOUNDARY_OF_CONVEX_HULL  = 1;
const rg_INT NON_BOUNDARY_OF_CONVEX_HULL = 0;

const rg_INT UNATTACHED = 0;
const rg_INT ATTACHED   = 1;
const rg_INT NOTTACHED  = 2; 

const rg_INT EXTRANEOUS_SIMPLEX = -1;
const rg_INT EXTERIOR_SIMPLEX = -1;
const rg_INT SINGULAR_SIMPLEX   = 0;
const rg_INT REGULAR_SIMPLEX    = 1;
const rg_INT INTERIOR_SIMPLEX   = 2;

const rg_INT NUM_VERTEX_ON_BCTETRAHEDRON = 4;
const rg_INT NUM_VERTEX_ON_BCFACE = 3;
const rg_INT NUM_VERTEX_ON_BCEDGE = 2;
const rg_INT NUM_EDGE_ON_BCTETRAHEDRON = 6;
const rg_INT NUM_EDGE_ON_BCFACE = 3;
const rg_INT NUM_FACE_ON_BCTETRAHEDRON = 4;



const string BCF_RECORD_SOURCE          = "SOURCE         ";
const string BCF_RECORD_PDBTIMESTAMP    = "PDBTIMESTAMP   ";
const string BCF_RECORD_PDBFILESIZE     = "PDBFILESIZE    ";
const string BCF_RECORD_DEFINE          = "DEFINE         ";
const string BCF_RECORD_DEFINE_RADIUS   = "DEFINE\tRADIUS ";
const string BCF_RECORD_VDGENENERATOR   = "VDGENERATOR    ";
const string BCF_RECORD_QTGENENERATOR   = "QTGENERATOR    ";
const string BCF_RECORD_BCFORMATVERSION = "BCFORMATVERSION";
const string BCF_RECORD_AUTHOR          = "AUTHOR         ";
const string BCF_RECORD_COMMENT         = "COMMENT";
const string BCF_RECORD_COMPLEX         = "BETA-COMPLEX   ";
const string BCF_RECORD_SHAPE           = "BETA-SHAPE     ";
const string BCF_RECORD_BETAVALUE       = "BETA-VALUE";
const string BCF_RECORD_MODEL           = "MODEL   ";
const string BCF_RECORD_REMARK          = "REMARK  ";
const string BCF_RECORD_VERTEX          = "BETAVTX ";
const string BCF_RECORD_EDGE            = "BETAEDGE";
const string BCF_RECORD_FACE            = "BETAFACE";
const string BCF_RECORD_CELL            = "BETACELL";
const string BCF_RECORD_ENDMODEL        = "ENDMODEL";

const string BCF_EXTERIOR_STATE = "E";
const string BCF_SINGULAR_STATE = "S";
const string BCF_REGULAR_STATE  = "R";
const string BCF_INTERIOR_STATE = "I";


#endif


