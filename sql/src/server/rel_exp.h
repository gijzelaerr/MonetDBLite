/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://monetdb.cwi.nl/Legal/MonetDBLicense-1.1.html
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2010 MonetDB B.V.
 * All Rights Reserved.
 */

#ifndef _REL_EXP_H_
#define _REL_EXP_H_

#define new_exp_list() list_create((fdestroy)&exp_destroy)

extern sql_exp *exp_dup(sql_exp* e);
extern void exp_destroy(sql_exp* e);

extern sql_exp *exp_compare( sql_exp *l, sql_exp *r, int cmptype);
extern sql_exp *exp_compare2( sql_exp *l, sql_exp *r, sql_exp *h, int cmptype);
extern sql_exp *exp_or( list *l, list *r);

#define exp_fromtype(e)	((list*)e->r)->h->data
#define exp_totype(e)	((list*)e->r)->h->next->data
extern sql_exp *exp_convert( sql_exp *exp, sql_subtype *fromtype, sql_subtype *totype );
extern str number2name(str s, int len, int i);
extern sql_exp *exp_op( list *l, sql_subfunc *f );

#define append(l,v) list_append(l,v) 
#define exp_unop(l,f) \
	exp_op(append(new_exp_list(),l), f)
#define exp_binop(l,r,f) \
	exp_op(append(append(new_exp_list(),l),r), f)
#define exp_op3(l,r,r2,f) \
	exp_op(append(append(append(new_exp_list(),l),r),r2), f)
#define exp_op4(l,r,r2,r3,f) \
	exp_op(append(append(append(append(new_exp_list(),l),r),r2),r3), f)
extern sql_exp *exp_aggr( list *l, sql_subaggr *a, int distinct, int no_nils, int card, int has_nil );
#define exp_aggr1(e, a, d, n, c, hn) \
	exp_aggr(append(new_exp_list(), e), a, d, n, c, hn)
extern sql_exp * exp_atom( atom *a);
extern sql_exp * exp_atom_bool(int b); 
extern sql_exp * exp_atom_int(int i);
extern sql_exp * exp_atom_lng(lng l);
extern sql_exp * exp_atom_wrd(wrd w);
extern sql_exp * exp_atom_str(str s, sql_subtype *st);
extern sql_exp * exp_atom_clob(str s);
extern sql_exp * exp_atom_ptr(void *s);
extern sql_exp * exp_atom_ref(int i, sql_subtype *tpe);
extern sql_exp * exp_param(char *name, sql_subtype *tpe, int frame);
extern sql_exp * exp_column( char *rname, char *name, sql_subtype *t, int card, int has_nils, int intern);
extern sql_exp * exp_alias( char *arname, char *acname, char *org_rname, char *org_cname, sql_subtype *t, int card, int has_nils, int intern);
extern void exp_setname( sql_exp *e, char *rname, char *name );
extern sql_exp* exp_label( sql_exp *e, int nr);

extern void exp_swap( sql_exp *e );

extern sql_subtype * exp_subtype( sql_exp *e );
extern char * exp_name( sql_exp *e );

extern char *exp_find_rel_name(sql_exp *e);

extern sql_exp *rel_find_exp( sql_rel *rel, sql_exp *e);

extern int exp_cmp( sql_exp *e1, sql_exp *e2);
extern int exp_match( sql_exp *e1, sql_exp *e2);
extern int exp_match_exp( sql_exp *e1, sql_exp *e2);
/* match just the column (cmp equality) expressions */
extern int exp_match_col_exps( sql_exp *e, list *l);
extern int exps_match_col_exps( sql_exp *e1, sql_exp *e2);
extern int exp_is_join(sql_exp *e);
extern int exp_is_eqjoin(sql_exp *e);
extern int exp_is_correlation(sql_exp *e, sql_rel *r );
extern int exp_is_join_exp(sql_exp *e);
extern int exp_is_atom(sql_exp *e);

extern sql_exp *exps_bind_column( list *exps, char *cname, int *ambiguous);
extern sql_exp *exps_bind_column2( list *exps, char *rname, char *cname);

extern int exps_card( list *l );
extern void exps_fix_card( list *exps, int card);
extern int exps_intern(list *exps);

extern char *compare_func( comp_type t );

#endif /* _REL_EXP_H_ */
