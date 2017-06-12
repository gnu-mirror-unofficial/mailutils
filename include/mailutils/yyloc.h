void mu_file_print_locus_point (FILE *,
				struct mu_locus_point const *lpt);
void mu_file_print_locus_range (FILE *,
				struct mu_locus_range const *loc);

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
  mu_file_print_locus_range (File, &(Loc))

    
  
