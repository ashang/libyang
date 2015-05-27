/**
 * @file printer/yang.c
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief YANG printer for libyang data model structure
 *
 * Copyright (c) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common.h"
#include "../tree.h"

#define INDENT ""
#define LEVEL (level*2)

static void yang_print_mnode(FILE *f, int level, struct ly_mnode *mnode,
                             int mask);

static void yang_print_text(FILE *f, int level, const char *name,
                            const char *text)
{
	const char *s, *t;

	fprintf(f, "%*s%s\n", LEVEL, INDENT, name);
	level++;

	fprintf(f, "%*s\"", LEVEL, INDENT);
	t = text;
	while((s = strchr(t, '\n'))) {
		fwrite(t, sizeof *t, s - t + 1, f);
		t = s + 1;
		fprintf(f, "%*s", LEVEL, INDENT);
	}

	fprintf(f, "%s\";\n\n", t);
	level--;

}

/*
 * Covers:
 * description, reference, status
 */
static void yang_print_mnode_common(FILE *f, int level, struct ly_mnode *mnode)
{
	if (mnode->flags & LY_NODE_STATUS_CURR) {
		fprintf(f, "%*sstatus \"current\";\n", LEVEL, INDENT);
	} else if (mnode->flags & LY_NODE_STATUS_DEPRC) {
		fprintf(f, "%*sstatus \"deprecated\";\n", LEVEL, INDENT);
	} else if (mnode->flags & LY_NODE_STATUS_OBSLT) {
		fprintf(f, "%*sstatus \"obsolete\";\n", LEVEL, INDENT);
	}

	if (mnode->dsc) {
		yang_print_text(f, level, "description", mnode->dsc);
	}
	if (mnode->ref) {
		yang_print_text(f, level, "reference", mnode->ref);
	}
}

/*
 * Covers:
 * config
 * description, reference, status
 */
static void yang_print_mnode_common2(FILE *f, int level, struct ly_mnode *mnode)
{
	if (!mnode->parent || (mnode->parent->flags & LY_NODE_CONFIG_MASK) != (mnode->flags & LY_NODE_CONFIG_MASK)) {
		/* print config only when it differs from the parent or in root */
		if (mnode->flags & LY_NODE_CONFIG_W) {
			fprintf(f, "%*sconfig \"true\";\n", LEVEL, INDENT);
		} else if (mnode->flags & LY_NODE_CONFIG_R) {
			fprintf(f, "%*sconfig \"false\";\n", LEVEL, INDENT);
		}
	}

	yang_print_mnode_common(f, level, mnode);
}

static void yang_print_type(FILE *f, int level, struct ly_module *module, struct ly_type *type)
{
	int i;

	if (type->prefix) {
		fprintf(f, "%*stype %s:%s {\n", LEVEL, INDENT, type->prefix, type->der->name);
	} else {
		fprintf(f, "%*stype %s {\n", LEVEL, INDENT, type->der->name);
	}
	level++;
	switch (type->base) {
	case LY_TYPE_ENUM:
		for (i = 0; i < type->info.enums.count; i++) {
			fprintf(f, "%*senum %s {\n", LEVEL, INDENT, type->info.enums.list[i].name);
			level++;
			yang_print_mnode_common(f, level, (struct ly_mnode *)&type->info.enums.list[i]);
			fprintf(f, "%*svalue %d;\n", LEVEL, INDENT, type->info.enums.list[i].value);
			level--;
			fprintf(f, "%*s}\n", LEVEL, INDENT);
		}
		break;
	case LY_TYPE_IDENT:
		if (module == type->info.ident.ref->module) {
			fprintf(f, "%*sbase %s;\n", LEVEL, INDENT, type->info.ident.ref->name);
		} else {
			fprintf(f, "%*sbase %s:%s;\n", LEVEL, INDENT, type->info.ident.ref->module->prefix, type->info.ident.ref->name);
		}
		break;
	default:
		/* TODO other cases */
		break;
	}
	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_typedef(FILE *f, int level, struct ly_module *module, struct ly_tpdf *tpdf)
{
	fprintf(f, "%*stypedef %s {\n", LEVEL, INDENT, tpdf->name);
	level++;

	yang_print_mnode_common(f, level, (struct ly_mnode *)tpdf);
	yang_print_type(f, level, module, &tpdf->type);

	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_identity(FILE *f, int level, struct ly_ident *ident)
{
	fprintf(f, "%*sidentity %s {\n", LEVEL, INDENT, ident->name);
	level++;

	yang_print_mnode_common(f, level, (struct ly_mnode *)ident);
	if (ident->base) {
		if (ident->base->module == ident->module) {
			fprintf(f, "%*sbase %s;\n", LEVEL, INDENT, ident->base->name);
		} else {
			fprintf(f, "%*sbase %s:%s;\n", LEVEL, INDENT, ident->base->module->prefix, ident->base->name);
		}
	}

	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);

}

static void yang_print_container(FILE *f, int level, struct ly_mnode *mnode)
{
	int i;
	struct ly_mnode *sub;
	struct ly_mnode_container *cont = (struct ly_mnode_container *)mnode;

	fprintf(f, "%*scontainer %s {\n", LEVEL, INDENT, mnode->name);
	level++;
	yang_print_mnode_common2(f, level, mnode);

	for (i = 0; i < cont->tpdf_size; i++) {
		yang_print_typedef(f, level, mnode->module, &cont->tpdf[i]);
	}

	LY_TREE_FOR(mnode->child, sub) {
		yang_print_mnode(f, level, sub, LY_NODE_CHOICE |LY_NODE_CONTAINER |
		                 LY_NODE_LEAF |LY_NODE_LEAFLIST | LY_NODE_LIST |
						 LY_NODE_USES | LY_NODE_GROUPING);
	}

	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_choice(FILE *f, int level, struct ly_mnode *mnode)
{
	struct ly_mnode *sub;

	fprintf(f, "%*schoice %s {\n", LEVEL, INDENT, mnode->name);
	level++;
	yang_print_mnode_common2(f, level, mnode);
	LY_TREE_FOR(mnode->child, sub) {
		yang_print_mnode(f, level, sub,
		                 LY_NODE_CONTAINER | LY_NODE_LEAF |
		                 LY_NODE_LEAFLIST | LY_NODE_LIST);
	}
	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_leaf(FILE *f, int level, struct ly_mnode *mnode)
{
	struct ly_mnode_leaf *leaf = (struct ly_mnode_leaf *)mnode;

	fprintf(f, "%*sleaf %s {\n", LEVEL, INDENT, mnode->name);
	level++;
	yang_print_mnode_common2(f, level, mnode);
	yang_print_type(f, level, mnode->module, &leaf->type);
	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_leaflist(FILE *f, int level, struct ly_mnode *mnode)
{
	struct ly_mnode_leaflist *llist = (struct ly_mnode_leaflist *)mnode;

	fprintf(f, "%*sleaf-list %s {\n", LEVEL, INDENT, mnode->name);
	level++;
	yang_print_mnode_common2(f, level, mnode);
	yang_print_type(f, level, mnode->module, &llist->type);
	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_list(FILE *f, int level, struct ly_mnode *mnode)
{
	int i;
	struct ly_mnode *sub;
	struct ly_mnode_list *list = (struct ly_mnode_list *)mnode;

	fprintf(f, "%*slist %s {\n", LEVEL, INDENT, mnode->name);
	level++;
	yang_print_mnode_common2(f, level, mnode);

	if (list->keys_size) {
		fprintf(f, "%*skey \"", LEVEL, INDENT);
		for (i = 0; i < list->keys_size; i++) {
			fprintf(f, "%s%s", list->keys[i]->name, i + 1 < list->keys_size ? " " : "");
		}
		fprintf(f, "\";\n");
	}

	for (i = 0; i < list->tpdf_size; i++) {
		yang_print_typedef(f, level, list->module, &list->tpdf[i]);
	}

	LY_TREE_FOR(mnode->child, sub) {
		yang_print_mnode(f, level, sub, LY_NODE_CHOICE |LY_NODE_CONTAINER |
		                 LY_NODE_LEAF |LY_NODE_LEAFLIST | LY_NODE_LIST |
						 LY_NODE_USES | LY_NODE_GROUPING);
	}
	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_grouping(FILE *f, int level, struct ly_mnode *mnode)
{
	int i;
	struct ly_mnode *node;
	struct ly_mnode_grp *grp = (struct ly_mnode_grp *)mnode;

	fprintf(f, "%*sgrouping %s {\n", LEVEL, INDENT, mnode->name);
	level++;

	yang_print_mnode_common(f, level, mnode);

	for (i = 0; i < grp->tpdf_size; i++) {
		yang_print_typedef(f, level, mnode->module, &grp->tpdf[i]);
	}

	LY_TREE_FOR(mnode->child, node) {
		yang_print_mnode(f, level, node, LY_NODE_CHOICE |LY_NODE_CONTAINER |
		                 LY_NODE_LEAF |LY_NODE_LEAFLIST | LY_NODE_LIST |
						 LY_NODE_USES | LY_NODE_GROUPING);
	}

	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_uses(FILE *f, int level, struct ly_mnode *mnode)
{
	struct ly_mnode_uses *uses = (struct ly_mnode_uses *)mnode;

	fprintf(f, "%*suses %s {\n", LEVEL, INDENT, uses->name);
	level++;

	yang_print_mnode_common(f, level, mnode);

	level--;
	fprintf(f, "%*s}\n", LEVEL, INDENT);
}

static void yang_print_mnode(FILE *f, int level, struct ly_mnode *mnode,
                             int mask)
{
	switch(mnode->nodetype & mask) {
	case LY_NODE_CONTAINER:
		yang_print_container(f, level, mnode);
		break;
	case LY_NODE_CHOICE:
		yang_print_choice(f, level, mnode);
		break;
	case LY_NODE_LEAF:
		yang_print_leaf(f, level, mnode);
		break;
	case LY_NODE_LEAFLIST:
		yang_print_leaflist(f, level, mnode);
		break;
	case LY_NODE_LIST:
		yang_print_list(f, level, mnode);
		break;
	case LY_NODE_USES:
		yang_print_uses(f, level, mnode);
		break;
	case LY_NODE_GROUPING:
		yang_print_grouping(f, level, mnode);
		break;
	default: break;
	}
}

int yang_print_model(FILE *f, struct ly_module *module)
{
	int i;
	int level = 0;
#define LEVEL (level*2)

	struct ly_mnode *mnode;

	fprintf(f, "module %s {\n", module->name);
	level++;

	fprintf(f, "%*snamespace \"%s\";\n", LEVEL, INDENT, module->ns);
	fprintf(f, "%*sprefix \"%s\";\n", LEVEL, INDENT, module->prefix);

	if (module->version) {
		fprintf(f, "%*syang-version \"%s\";\n", LEVEL, INDENT, module->version == 1 ? "1.0" : "1.1");
	}

	for (i = 0; i < module->imp_size; i++) {
		fprintf(f, "%*simport \"%s\" {\n", LEVEL, INDENT,
				module->imp[i].module->name);
		level++;
		yang_print_text(f, level, "prefix", module->imp[i].prefix);
		if (module->imp[i].rev[0]) {
			yang_print_text(f, level, "revision-date", module->imp[i].rev);
		}
		level--;
		fprintf(f, "%*s}\n", LEVEL, INDENT);
	}

	for (i = 0; i < module->inc_size; i++) {
		if (module->inc[i].rev[0]) {
			fprintf(f, "%*sinclude \"%s\" {\n", LEVEL, INDENT,
					module->inc[i].submodule->name);
			yang_print_text(f, level + 1, "revision-date", module->imp[i].rev);
			fprintf(f, "%*s}\n", LEVEL, INDENT);
		} else {
			fprintf(f, "%*sinclude \"%s\";\n", LEVEL, INDENT,
					module->inc[i].submodule->name);
		}
	}

	if (module->org) {
		yang_print_text(f, level, "organization", module->org);
	}
	if (module->contact) {
		yang_print_text(f, level, "contact", module->contact);
	}
	if (module->dsc) {
		yang_print_text(f, level, "description", module->dsc);
	}
	if (module->ref) {
		yang_print_text(f, level, "reference", module->ref);
	}
	for (i = 0; i < module->rev_size; i++) {
		if (module->rev[i].dsc || module->rev[i].ref) {
			fprintf(f, "%*srevision \"%s\" {\n", LEVEL, INDENT,
					module->rev[i].date);
			level++;
			if (module->rev[i].dsc) {
				yang_print_text(f, level, "description", module->rev[i].dsc);
			}
			if (module->rev[i].ref) {
				yang_print_text(f, level, "reference", module->rev[i].ref);
			}
			level--;
			fprintf(f, "%*s}\n", LEVEL, INDENT);
		} else {
			yang_print_text(f, level, "revision", module->rev[i].date);
		}
	}

	for (i = 0; i < module->ident_size; i++) {
		yang_print_identity(f, level, &module->ident[i]);
	}

	for (i = 0; i < module->tpdf_size; i++) {
		yang_print_typedef(f, level, module, &module->tpdf[i]);
	}

	LY_TREE_FOR(module->data, mnode) {
		yang_print_mnode(f, level, mnode, LY_NODE_CHOICE |LY_NODE_CONTAINER |
                         LY_NODE_LEAF |LY_NODE_LEAFLIST | LY_NODE_LIST |
						 LY_NODE_USES | LY_NODE_GROUPING);
	}

	fprintf(f, "}\n");

	return EXIT_SUCCESS;
#undef LEVEL
}
