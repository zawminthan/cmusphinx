/*
 * lc.c -- Left context transformations
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * 07-Apr-1997	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Started.
 */


/*
 * Input lines of the form:
 *   354 .PERIOD                  => (TD)  p  ih  r  iy  ax  dd   ()
 *     9 IN                       => (S)   [[ AX  => IX  ]] n   (F)
 *     6 HELP PAY                 => (AX)  hh  eh  l  [[ => PD  ]] p  ey   (F)
 * Output 1st phone transformations only.
 */

#include <libutil/libutil.h>

static char *phonestr[] = {
    "AA",
    "AE",
    "AH",
    "AO",
    "AW",
    "AX",
    "AXR",
    "AY",
    "B",
    "BD",
    "CH",
    "D",
    "DD",
    "DH",
    "DX",
    "EH",
    "ER",
    "EY",
    "F",
    "G",
    "GD",
    "HH",
    "IH",
    "IX",
    "IY",
    "JH",
    "K",
    "KD",
    "L",
    "M",
    "N",
    "NG",
    "OW",
    "OY",
    "P",
    "PD",
    "R",
    "S",
    "SH",
    "T",
    "TD",
    "TH",
    "TS",
    "UH",
    "UW",
    "V",
    "W",
    "Y",
    "Z",
    "ZH",
    "--",	/* Dummy empty-phone */
   NULL
};


static int32 phone_str2id (char *str)
{
    int32 i;
    
    ucase (str);
    
    for (i = 0; phonestr[i] && (strcmp (phonestr[i], str) != 0); i++);
    if (! phonestr[i])
	E_FATAL("Unknown phone: %s\n", str);
    
    return i;
}


static int32 tmpp1;
static int32 *tmpcount;

static int32 cmp_count (int32 *a, int32 *b)
{
    if (tmpcount[*b] > tmpcount[*a])
	return 1;
    else if (tmpcount[*b] < tmpcount[*a])
	return -1;
    else if (*a == tmpp1)
	return -1;
    else if (*b == tmpp1)
	return 1;
    else
	return 0;
}


#define MAX_WORDS	4092

main (int32 argc, char *argv[])
{
    char line[16384], **wptr;
    int32 i, j, k, prevk, n, np, ****mapcount, *mapid;
    int32 p1, lc, rc, p2;

    if (argc > 1) {
	E_INFO("Usage: %s < <result-of-pronerralign>\n", argv[0]);
	exit(0);
    }
    
    for (np = 0; phonestr[np]; np++);
    
    E_INFO("%d phones\n", np-1);
    E_INFO("Allocating %d x %d x %d x %d mapcount array\n", np, np, np, np);
    mapcount = (int32 ****) ckd_calloc_3d (np, np, np, sizeof(int32 *));
    mapid = (int32 *) ckd_calloc (np, sizeof(int32));
    tmpcount = (int32 *) ckd_calloc (np, sizeof(int32));
    for (i = 0; i < np; i++) {
	for (j = 0; j < np; j++) {
	    for (k = 0; k < np; k++) {
		mapcount[i][j][k] = (int32 *) ckd_calloc (np, sizeof(int32));
	    }
	}
    }

    wptr = (char **) ckd_calloc (MAX_WORDS, sizeof(char *));
    
    prevk = 0;
    
    while (fgets (line, sizeof(line), stdin) != NULL) {
	if ((n = str2words (line, wptr, MAX_WORDS)) < 0)
	    E_FATAL("str2words(%s) failed; increase %d(?)\n", line, MAX_WORDS);
	
	/* Read first (count) field */
	if (n == 0) continue;
	if (sscanf (wptr[0], "%d", &k) != 1)
	    E_FATAL("First field not a count: %s\n", wptr[0]);
	if (k != prevk) {
	    printf (" %d", k);
	    fflush (stdout);
	    prevk = k;
	}
	if (k <= 0)
	    break;
	
	/* Find => separator after word list */
	for (i = 0; (i < n) && (strcmp (wptr[i], "=>") != 0); i++);
	i++;		/* Hopefully at (lc) */
	j = strlen(wptr[i]) - 1;
	assert ((wptr[i][0] == '(') && (wptr[i][j] == ')'));
	if (strcmp (wptr[i], "()") == 0)
	    strcpy (wptr[i], "--");
	else {
	    wptr[i][j] = '\0';
	    (wptr[i])++;
	}
	
	/* Must have at least: (lc) p1 p2 (rc) */
	if (n-i <= 3)
	    continue;	
	assert (i > 2);
	
	if ((strcmp (wptr[i+1], "[[") != 0) && (strcmp (wptr[i+2], "[[") != 0)) {
	    /* No error */
#if 0
	    printf ("%6d %-5s %-5s %-5s          %s\n",
		    k, wptr[i], wptr[i+1], wptr[i+2], wptr[1]);
#endif

	    lc = phone_str2id (wptr[i]);
	    p1 = phone_str2id (wptr[i+1]);
	    rc = phone_str2id (wptr[i+2]);
	    p2 = p1;
	    mapcount[p1][lc][rc][p2] += k;
	} else if (strcmp (wptr[i+1], "[[") == 0) {
	    /*
	     * First phone got transformed.  Must be:
	     *     (lc) [[ => ee ]] p2 (rc),
	     *     (lc) [[ ee => ]] p2 (rc), or
	     *     (lc) [[ pp => ee ]] p2 (rc)
	     */
	    if (n-i <= 6)
		continue;
	    
	    if ((strcmp (wptr[i+2], "=>") == 0) &&
		(strcmp (wptr[i+4], "]]") == 0) &&
		(strcmp (wptr[i+5], "[[") != 0)) {	/* (lc) [[ => ee ]] p2 (rc) */
#if 0
		printf ("%6d %-5s %-5s %-5s => %-5s %s\n",
			k, wptr[i], wptr[i+3], wptr[i+5], "--", wptr[1]);
#endif

		lc = phone_str2id (wptr[i]);
		p1 = phone_str2id (wptr[i+3]);
		rc = phone_str2id (wptr[i+5]);
		p2 = np-1;
		mapcount[p1][lc][rc][p2] += k;
	    } else if ((strcmp (wptr[i+3], "=>") == 0) &&
		       (strcmp (wptr[i+4], "]]") == 0) &&
		       (strcmp (wptr[i+5], "[[") != 0)) { /* (lc) [[ ee => ]] p2 (rc) */
#if 0
		printf ("%6d %-5s %-5s %-5s => %-5s %s\n",
			k, wptr[i], "--", wptr[i+5], wptr[i+2], wptr[1]);
#endif

		lc = phone_str2id (wptr[i]);
		p1 = np-1;
		rc = phone_str2id (wptr[i+5]);
		p2 = phone_str2id (wptr[i+2]);
		mapcount[p1][lc][rc][p2] += k;
	    } else if ((strcmp (wptr[i+3], "=>") == 0) &&
		       (strcmp (wptr[i+5], "]]") == 0) &&
		       (strcmp (wptr[i+6], "[[") != 0) &&
		       (n-i > 7)) {			/* (lc) [[ pp => ee ]] p2 (rc) */
#if 0
		printf ("%6d %-5s %-5s %-5s => %-5s %s\n",
			k, wptr[i], wptr[i+4], wptr[i+6], wptr[i+2], wptr[1]);
#endif
		lc = phone_str2id (wptr[i]);
		p1 = phone_str2id (wptr[i+4]);
		rc = phone_str2id (wptr[i+6]);
		p2 = phone_str2id (wptr[i+2]);
		mapcount[p1][lc][rc][p2] += k;
	    }
	}
    }

    printf ("\n");
    
    for (p1 = 0; p1 < np; p1++) {
	for (lc = 0; lc < np; lc++) {
	    for (rc = 0; rc < np; rc++) {
		k = 0;
		for (p2 = 0; p2 < np; p2++)
		    k += mapcount[p1][lc][rc][p2];
		if (k > 0) {
		    printf ("%-4s %-4s %-4s %5d",
			    phonestr[p1], phonestr[lc], phonestr[rc], k);
		    
		    for (p2 = 0; p2 < np; p2++) {
			tmpcount[p2] = mapcount[p1][lc][rc][p2];
			mapid[p2] = p2;
		    }
		    tmpp1 = p1;
		    qsort (mapid, np, sizeof(int32), cmp_count);
		    
		    for (p2 = 0; p2 < np; p2++) {
			if (mapcount[p1][lc][rc][mapid[p2]])
			    printf (" %s %d",
				    phonestr[mapid[p2]], mapcount[p1][lc][rc][mapid[p2]]);
		    }
		    printf ("\n");
		}
	    }
	}
    }
}
