#define YYLTYPE struct mu_locus_range
#define YYLLOC_DEFAULT(Current, Rhs, N)				  \
  do								  \
    {								  \
      if (N)							  \
	{							  \
	  (Current).beg = YYRHSLOC(Rhs, 1).beg;			  \
	  (Current).end = YYRHSLOC(Rhs, N).end;			  \
	}							  \
      else							  \
	{							  \
	  (Current).beg = YYRHSLOC(Rhs, 0).end;			  \
	  (Current).end = (Current).beg;			  \
	}							  \
    } while (0)
#define YY_LOCATION_PRINT(File, Loc)     		        \
  do								\
    {								\
      if (!mu_locus_point_same_file (&(Loc).beg, &(Loc).end))	\
	fprintf (File, "%s:%u.%u-%s:%u.%u",			\
	         (Loc).beg.mu_file,				\
                 (Loc).beg.mu_line, (Loc).beg.mu_col,		\
                 (Loc).end.mu_file,				\
        	 (Loc).end.mu_line, (Loc).end.mu_col);	        \
      else if ((Loc).beg.mu_line != (Loc).end.mu_line)		\
	fprintf (File, "%s:%u.%u-%u.%u",			\
		 (Loc).beg.mu_file,				\
		 (Loc).beg.mu_line, (Loc).beg.mu_col,		\
		 (Loc).end.mu_line, (Loc).end.mu_col);		\
      else if ((Loc).beg.mu_col != (Loc).end.mu_col)		\
	fprintf (File, "%s:%u.%u-%u",				\
		 (Loc).beg.mu_file,				\
		 (Loc).beg.mu_line, (Loc).beg.mu_col,		\
		 (Loc).end.mu_col);				\
      else							\
	fprintf (File, "%s:%u.%u",				\
		 (Loc).beg.mu_file,				\
		 (Loc).beg.mu_line, (Loc).beg.mu_col);		\
} while (0)

    
  
