/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2012, The University of Texas

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis2.h"

#define FUNCPTR_T gemm_fp

typedef void (*FUNCPTR_T)(
                           doff_t  diagoffa,
                           dim_t   m,
                           dim_t   n,
                           dim_t   k,
                           void*   a, inc_t rs_a, inc_t cs_a, inc_t ps_a,
                           void*   b, inc_t rs_b, inc_t cs_b, inc_t ps_b,
                           void*   c, inc_t rs_c, inc_t cs_c
                         );

static FUNCPTR_T GENARRAY(ftypes,trsm_l_ker_var3);


void bl2_trsm_l_ker_var3( obj_t*  alpha,
                          obj_t*  a,
                          obj_t*  b,
                          obj_t*  beta,
                          obj_t*  c,
                          trsm_t* cntl )
{
	num_t     dt_exec   = bl2_obj_execution_datatype( *c );

	doff_t    diagoffa  = bl2_obj_diag_offset( *a );

	dim_t     m         = bl2_obj_length( *c );
	dim_t     n         = bl2_obj_width( *c );
	dim_t     k         = bl2_obj_width( *a );

	void*     buf_a     = bl2_obj_buffer_at_off( *a );
	inc_t     rs_a      = bl2_obj_row_stride( *a );
	inc_t     cs_a      = bl2_obj_col_stride( *a );
	inc_t     ps_a      = bl2_obj_panel_stride( *a );

	void*     buf_b     = bl2_obj_buffer_at_off( *b );
	inc_t     rs_b      = bl2_obj_row_stride( *b );
	inc_t     cs_b      = bl2_obj_col_stride( *b );
	inc_t     ps_b      = bl2_obj_panel_stride( *b );

	void*     buf_c     = bl2_obj_buffer_at_off( *c );
	inc_t     rs_c      = bl2_obj_row_stride( *c );
	inc_t     cs_c      = bl2_obj_col_stride( *c );

	FUNCPTR_T f;

/*
	// Handle the special case where c and a are complex and b is real.
	// Note that this is the ONLY case allowed by the inner kernel whereby
	// the datatypes of a and b differ. In this situation, the execution
	// datatype is real, so we need to inflate (by a factor of two):
	//  - the m dimension,
	//  - the column stride of c,
	//  - the column stride (ie: the panel length) of a, and
	//  - the panel stride of a.
	if ( bl2_obj_is_complex( *a ) && bl2_obj_is_real( *b ) )
	{
		m    *= 2;
		cs_c *= 2;
		cs_a *= 2;
		ps_a *= 2;
	}
*/

	// Index into the type combination array to extract the correct
	// function pointer.
	f = ftypes[dt_exec];

	// Invoke the function.
	f( diagoffa,
	   m,
	   n,
	   k,
	   buf_a, rs_a, cs_a, ps_a,
	   buf_b, rs_b, cs_b, ps_b,
	   buf_c, rs_c, cs_c );
}


#undef  GENTFUNC
#define GENTFUNC( ctype, ch, varname, trsmukr, gemmukr ) \
\
void PASTEMAC(ch,varname)( \
                           doff_t  diagoffa, \
                           dim_t   m, \
                           dim_t   n, \
                           dim_t   k, \
                           void*   a, inc_t rs_a, inc_t cs_a, inc_t ps_a, \
                           void*   b, inc_t rs_b, inc_t cs_b, inc_t ps_b, \
                           void*   c, inc_t rs_c, inc_t cs_c \
                         ) \
{ \
	/* Temporary buffer for duplicating elements of B. */ \
	ctype           bd[ PASTEMAC2(ch,varname,_kc) * \
	                    PASTEMAC2(ch,varname,_nr) * \
	                    PASTEMAC2(ch,varname,_ndup) ]; \
	ctype* restrict bp; \
\
	/* Temporary C buffer for edge cases. */ \
	ctype           ct[ PASTEMAC2(ch,varname,_mr) * \
	                    PASTEMAC2(ch,varname,_nr) ]; \
	const inc_t     rs_ct = 1; \
	const inc_t     cs_ct = PASTEMAC2(ch,varname,_mr); \
\
	/* Alias constants to shorter names. */ \
	const dim_t     MR        = PASTEMAC2(ch,varname,_mr); \
	const dim_t     NR        = PASTEMAC2(ch,varname,_nr); \
	const bool_t    DUPB      = PASTEMAC2(ch,varname,_dupb); \
	const dim_t     NDUP      = PASTEMAC2(ch,varname,_ndup); \
\
	ctype* restrict one       = PASTEMAC(ch,1); \
	ctype* restrict zero      = PASTEMAC(ch,0); \
	ctype* restrict minus_one = PASTEMAC(ch,m1); \
	ctype* restrict a_cast    = a; \
	ctype* restrict b_cast    = b; \
	ctype* restrict c_cast    = c; \
	ctype* restrict a1; \
	ctype* restrict b1; \
	ctype* restrict c1; \
	ctype* restrict c11; \
	ctype* restrict b11; \
	ctype* restrict a10; \
	ctype* restrict a11; \
	ctype* restrict bp01; \
	ctype* restrict bp11; \
	ctype* restrict bp_i; \
\
	doff_t          diagoffa_i; \
	dim_t           m_iter, m_left; \
	dim_t           n_iter, n_left; \
	dim_t           m_cur; \
	dim_t           n_cur; \
	dim_t           k_nr; \
	dim_t           k_a1011; \
	dim_t           k_a10; \
	dim_t           off_a1011, off_b11; \
	dim_t           i, j; \
	dim_t           rstep_a; \
	dim_t           rstep_b, cstep_b; \
	dim_t           rstep_c, cstep_c; \
\
	/*
	   Assumptions/assertions:
         rs_a == 1
	     cs_a == GEMM_MR
	     ps_a == stride to next row panel of A
         rs_b == GEMM_NR
	     cs_b == 1
	     ps_b == stride to next column panel of B
         rs_c == (no assumptions)
	     cs_c == (no assumptions)
	*/ \
\
	/* If any dimension is zero, return immediately. */ \
	if ( bl2_zero_dim3( m, n, k ) ) return; \
\
	/* Safeguard: If matrix A is above the diagonal, it is implicitly zero.
	   So we do nothing. */ \
	if ( bl2_is_strictly_above_diag_n( diagoffa, m, k ) ) return; \
\
	/* The first thing we do is check the k dimension, which needs to be
	   a multiple of MR. If k isn't a multiple of MR, we adjust it higher.
	   This allows us to use a single micro-kernel, which performs an
	   MR x MR triangular solve, even for cases when k isn't actually a
	   multiple of MR. The key is that when A was packed, its edges were
	   first zero padded, and further, the panel that stores the bottom-
	   right corner of the matrix has its diagonal that extendeds into
	   the zero padded region as identity. This allows the trsm of that
	   bottom-right panel to proceed without producing any infs or NaNs
	   or any other numerical funny business that would infect the "good"
	   values of the corresponding block of B. */ \
	if ( k % MR != 0 ) k += MR - ( k % MR ); \
\
	/* If the diagonal offset is negative, adjust the pointer to C and
	   treat this case as if the diagonal offset were zero. Note that
	   we don't need to adjust the pointer to A since packm would have
	   simply skipped over the panels that were not stored. */ \
	if ( diagoffa < 0 ) \
	{ \
		i        = -diagoffa; \
		m        = m - i; \
		diagoffa = 0; \
		c_cast   = c_cast + (i  )*rs_c; \
	} \
\
	/* Clear the temporary C buffer in case it has any infs or NaNs. */ \
	PASTEMAC(ch,set0_mxn)( MR, NR, \
	                       ct, rs_ct, cs_ct ); \
\
	/* Compute number of primary and leftover components of the m and n
       dimensions. */ \
	n_iter  = n / NR; \
	n_left  = n % NR; \
\
	m_iter  = m / MR; \
	m_left  = m % MR; \
\
	if ( n_left ) ++n_iter; \
	if ( m_left ) ++m_iter; \
\
	/* Compute the number of elements in B to duplicate per iteration. */ \
	k_a1011 = bl2_min( k, diagoffa + m ); \
	k_nr    = k_a1011 * NR; \
\
	/* Determine some increments used to step through A, B, and C. */ \
	rstep_a = k * MR; \
\
	rstep_b = NR * MR; \
	cstep_b = ps_b; \
\
	rstep_c = rs_c * MR; \
	cstep_c = cs_c * NR; \
\
	off_b11 = diagoffa; \
\
	b1 = b_cast; \
	c1 = c_cast; \
\
	/* If the micro-kernel needs elements of B duplicated, set bp to
	   point to the duplication buffer. If no duplication is called for,
	   bp will be set to the current column panel of B for each iteration
	   of the outer loop below. */ \
	if ( DUPB ) bp = bd; \
\
	/* Loop over the n dimension (NR columns at a time). */ \
	for ( j = 0; j < n_iter; ++j ) \
	{ \
		a1  = a_cast; \
		c11 = c1; \
		b11 = b1 + (off_b11  )*NR; \
\
		n_cur = ( bl2_is_not_edge_f( j, n_iter, n_left ) ? NR : n_left ); \
\
		/* If duplication is needed, copy the current iteration's NR
		   columns of B to a local buffer with each value duplicated. */ \
		if ( DUPB ) PASTEMAC(ch,dupl)( k_nr, b1, bd ); \
		else        bp = b1; \
\
		/* Loop over the m dimension (MR rows at a time). */ \
		for ( i = 0; i < m_iter; ++i ) \
		{ \
			diagoffa_i = diagoffa + ( doff_t )i*MR; \
\
			m_cur = ( bl2_is_not_edge_f( i, m_iter, m_left ) ? MR : m_left ); \
\
			/* If the current panel of A intersects the diagonal, use a
			   special micro-kernel that performs a fused gemm and trsm.
			   If the current panel of A resides below the diagonal, use a
			   a regular gemm micro-kernel. Otherwise, if it is above the
			   diagonal, it was not packed (because it is implicitly zero)
			   and so we do nothing. */ \
			if ( bl2_intersects_diag_n( diagoffa_i, MR, k ) ) \
			{ \
				/* Determine the offset to the beginning of the panel that
				   was packed so we can index into the corresponding location
				   in bp. Then compute the length of that panel. */ \
				off_a1011 = 0; \
				k_a1011   = bl2_min( k, diagoffa_i + MR ); \
				k_a10     = k_a1011 - MR; \
\
				bp_i = bp + off_a1011 * NR * NDUP; \
\
				/* Compute the addresses of the A10 panel and triangular
				   block A11, and the corresponding panel Bd01 and block
				   Bd11. */ \
				a10  = a1; \
				a11  = a1   + k_a10 * MR; \
				bp01 = bp_i; \
				bp11 = bp_i + k_a10 * NR * NDUP; \
\
				/* Handle interior and edge cases separately. */ \
				if ( m_cur == MR && n_cur == NR ) \
				{ \
					/* Invoke the gemm and trsm micro-kernels. */ \
					PASTEMAC(ch,gemmukr)( k_a10, \
					                      minus_one, \
					                      a10, \
					                      bp01, \
					                      one, \
					                      b11, rs_b, cs_b ); \
					PASTEMAC(ch,trsmukr)( a11, \
					                      b11, \
					                      bp11, \
					                      c11, rs_c, cs_c ); \
				} \
				else \
				{ \
					/* Invoke the gemm and trsm micro-kernels. */ \
					PASTEMAC(ch,gemmukr)( k_a10, \
					                      minus_one, \
					                      a10, \
					                      bp01, \
					                      one, \
					                                 b11, rs_b, cs_b ); \
					PASTEMAC(ch,trsmukr)( a11, \
					                      b11, \
					                      bp11, \
					                      ct, rs_ct, cs_ct ); \
\
					/* Copy the result to the bottom edge of C. */ \
					PASTEMAC2(ch,ch,copys_mxn)( m_cur, n_cur, \
					                            ct,  rs_ct, cs_ct, \
					                            c11, rs_c,  cs_c ); \
				} \
\
				a1 += k_a1011 * MR; \
			} \
			else if ( bl2_is_strictly_below_diag_n( diagoffa_i, MR, k ) ) \
			{ \
				/* Handle interior and edge cases separately. */ \
				if ( m_cur == MR && n_cur == NR ) \
				{ \
					/* Invoke the gemm micro-kernel. */ \
					PASTEMAC(ch,gemmukr)( k, \
					                      minus_one, \
					                      a1, \
					                      bp, \
					                      one, \
					                      c11, rs_c, cs_c ); \
				} \
				else \
				{ \
					/* Invoke the gemm micro-kernel. */ \
					PASTEMAC(ch,gemmukr)( k, \
					                      minus_one, \
					                      a1, \
					                      bp, \
					                      zero, \
					                      ct, rs_ct, cs_ct ); \
\
					/* Add the result to the edge of C. */ \
					PASTEMAC2(ch,ch,adds_mxn)( m_cur, n_cur, \
					                           ct,  rs_ct, cs_ct, \
					                           c11, rs_c,  cs_c ); \
				} \
\
				a1 += rstep_a; \
			} \
\
			b11  += rstep_b; \
			c11  += rstep_c; \
		} \
\
		b1 += cstep_b; \
		c1 += cstep_c; \
	} \
}

INSERT_GENTFUNC_BASIC2( trsm_l_ker_var3, TRSM_L_UKERNEL, GEMM_UKERNEL )
