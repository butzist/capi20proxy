/*
 * hash.c: chained hash tables
 *
 * Reference: Your favorite introductory book on algorithms
 *
 * Copyright (C) 2000 Bjorn Reese and Daniel Veillard.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
 * CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
 *
 * Author: bjorn.reese@systematic.dk
 */

#ifdef WIN32
#include "win32config.h"
#else
#include "config.h"
#endif

#include <string.h>
#include <libxml/hash.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/*
 * A single entry in the hash table
 */
typedef struct _xmlHashEntry xmlHashEntry;
typedef xmlHashEntry *xmlHashEntryPtr;
struct _xmlHashEntry {
    struct _xmlHashEntry *next;
    xmlChar *name;
    xmlChar *name2;
    xmlChar *name3;
    void *payload;
};

/*
 * The entire hash table
 */
struct _xmlHashTable {
    struct _xmlHashEntry **table;
    int size;
};

/*
 * xmlHashComputeKey:
 * Calculate the hash key
 */
static unsigned long
xmlHashComputeKey(xmlHashTablePtr table, const xmlChar *string) {
    unsigned long value = 0L;
    char ch;

    while ((ch = *string++) != 0) {
        /* value *= 31; */
        value += (unsigned long)ch;
    }
    return (value % table->size);
}

/**
 * xmlHashCreate:
 * @size: the size of the hash table
 *
 * Create a new xmlHashTablePtr.
 *
 * Returns the newly created object, or NULL if an error occured.
 */
xmlHashTablePtr
xmlHashCreate(int size) {
    xmlHashTablePtr table;

    if (size <= 0)
        size = 256;

    table = xmlMalloc(sizeof(xmlHashTable));
    if (table) {
        table->size = size;
        table->table = xmlMalloc(size * sizeof(xmlHashEntry));
        if (table->table) {
  	    memset(table->table, 0, size * sizeof(xmlHashEntry));
  	    return(table);
        }
        xmlFree(table);
    }
    return(NULL);
}

/**
 * xmlHashFree:
 * @table: the hash table
 * @f:  the deallocator function for items in the hash
 *
 * Free the hash table and its contents. The userdata is
 * deallocated with f if provided.
 */
void
xmlHashFree(xmlHashTablePtr table, xmlHashDeallocator f) {
    int i;
    xmlHashEntryPtr iter;
    xmlHashEntryPtr next;

    if (table == NULL)
	return;
    if (table->table) {
	for(i = 0; i < table->size; i++) {
	    iter = table->table[i];
	    while (iter) {
		next = iter->next;
		if (iter->name)
		    xmlFree(iter->name);
		if (iter->name2)
		    xmlFree(iter->name2);
		if (iter->name3)
		    xmlFree(iter->name3);
		if (f)
		    f(iter->payload, iter->name);
		iter->payload = NULL;
		xmlFree(iter);
		iter = next;
	    }
	    table->table[i] = NULL;
	}
	xmlFree(table->table);
    }
    xmlFree(table);
}

/**
 * xmlHashAddEntry:
 * @table: the hash table
 * @name: the name of the userdata
 * @userdata: a pointer to the userdata
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the name. Duplicate names generate errors.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashAddEntry(xmlHashTablePtr table, const xmlChar *name, void *userdata) {
    return(xmlHashAddEntry3(table, name, NULL, NULL, userdata));
}

/**
 * xmlHashAddEntry2:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 * @userdata: a pointer to the userdata
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the (name, name2) tuple. Duplicate tuples generate errors.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashAddEntry2(xmlHashTablePtr table, const xmlChar *name,
	        const xmlChar *name2, void *userdata) {
    return(xmlHashAddEntry3(table, name, name2, NULL, userdata));
}

/**
 * xmlHashUpdateEntry:
 * @table: the hash table
 * @name: the name of the userdata
 * @userdata: a pointer to the userdata
 * @f: the deallocator function for replaced item (if any)
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the name. Existing entry for this name will be removed
 * and freed with @f if found.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashUpdateEntry(xmlHashTablePtr table, const xmlChar *name,
	           void *userdata, xmlHashDeallocator f) {
    return(xmlHashUpdateEntry3(table, name, NULL, NULL, userdata, f));
}

/**
 * xmlHashUpdateEntry2:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 * @userdata: a pointer to the userdata
 * @f: the deallocator function for replaced item (if any)
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the (name, name2) tuple. Existing entry for this tuple will
 * be removed and freed with @f if found.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashUpdateEntry2(xmlHashTablePtr table, const xmlChar *name,
	           const xmlChar *name2, void *userdata,
		   xmlHashDeallocator f) {
    return(xmlHashUpdateEntry3(table, name, name2, NULL, userdata, f));
}

/**
 * xmlHashLookup:
 * @table: the hash table
 * @name: the name of the userdata
 *
 * Find the userdata specified by the name.
 *
 * Returns the a pointer to the userdata
 */
void *
xmlHashLookup(xmlHashTablePtr table, const xmlChar *name) {
    return(xmlHashLookup3(table, name, NULL, NULL));
}

/**
 * xmlHashLookup2:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 *
 * Find the userdata specified by the (name, name2) tuple.
 *
 * Returns the a pointer to the userdata
 */
void *
xmlHashLookup2(xmlHashTablePtr table, const xmlChar *name,
	      const xmlChar *name2) {
    return(xmlHashLookup3(table, name, name2, NULL));
}

/**
 * xmlHashAddEntry3:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 * @name3: a third name of the userdata
 * @userdata: a pointer to the userdata
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the tuple (name, name2, name3). Duplicate entries generate
 * errors.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashAddEntry3(xmlHashTablePtr table, const xmlChar *name,
	         const xmlChar *name2, const xmlChar *name3,
		 void *userdata) {
    unsigned long key;
    xmlHashEntryPtr entry;
    xmlHashEntryPtr insert;

    if ((table == NULL) || name == NULL)
	return(-1);

    /*
     * Check for duplicate and insertion location.
     */
    key = xmlHashComputeKey(table, name);
    if (table->table[key] == NULL) {
	insert = NULL;
    } else {
	for (insert = table->table[key]; insert->next != NULL;
	     insert = insert->next) {
	    if ((xmlStrEqual(insert->name, name)) &&
		(xmlStrEqual(insert->name2, name2)) &&
		(xmlStrEqual(insert->name3, name3)))
		return(-1);
	}
	if ((xmlStrEqual(insert->name, name)) &&
	    (xmlStrEqual(insert->name2, name2)) &&
	    (xmlStrEqual(insert->name3, name3)))
	    return(-1);
    }

    entry = xmlMalloc(sizeof(xmlHashEntry));
    if (entry == NULL)
	return(-1);
    entry->name = xmlStrdup(name);
    entry->name2 = xmlStrdup(name2);
    entry->name3 = xmlStrdup(name3);
    entry->payload = userdata;
    entry->next = NULL;


    if (insert == NULL) {
	table->table[key] = entry;
    } else {
	insert->next = entry;
    }
    return(0);
}

/**
 * xmlHashUpdateEntry3:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 * @name3: a third name of the userdata
 * @userdata: a pointer to the userdata
 * @f: the deallocator function for replaced item (if any)
 *
 * Add the userdata to the hash table. This can later be retrieved
 * by using the tuple (name, name2, name3). Existing entry for this tuple
 * will be removed and freed with @f if found.
 *
 * Returns 0 the addition succeeded and -1 in case of error.
 */
int
xmlHashUpdateEntry3(xmlHashTablePtr table, const xmlChar *name,
	           const xmlChar *name2, const xmlChar *name3,
		   void *userdata, xmlHashDeallocator f) {
    unsigned long key;
    xmlHashEntryPtr entry;
    xmlHashEntryPtr insert;

    if ((table == NULL) || name == NULL)
	return(-1);

    /*
     * Check for duplicate and insertion location.
     */
    key = xmlHashComputeKey(table, name);
    if (table->table[key] == NULL) {
	insert = NULL;
    } else {
	for (insert = table->table[key]; insert->next != NULL;
	     insert = insert->next) {
	    if ((xmlStrEqual(insert->name, name)) &&
		(xmlStrEqual(insert->name2, name2)) &&
		(xmlStrEqual(insert->name3, name3))) {
		if (f)
		    f(insert->payload, insert->name);
		insert->payload = userdata;
		return(0);
	    }
	}
	if ((xmlStrEqual(insert->name, name)) &&
	    (xmlStrEqual(insert->name2, name2)) &&
	    (xmlStrEqual(insert->name3, name3))) {
	    if (f)
		f(insert->payload, insert->name);
	    insert->payload = userdata;
	    return(0);
	}
    }

    entry = xmlMalloc(sizeof(xmlHashEntry));
    if (entry == NULL)
	return(-1);
    entry->name = xmlStrdup(name);
    entry->name2 = xmlStrdup(name2);
    entry->name3 = xmlStrdup(name3);
    entry->payload = userdata;
    entry->next = NULL;


    if (insert == NULL) {
	table->table[key] = entry;
    } else {
	insert->next = entry;
    }
    return(0);
}

/**
 * xmlHashLookup:
 * @table: the hash table
 * @name: the name of the userdata
 * @name2: a second name of the userdata
 * @name3: a third name of the userdata
 *
 * Find the userdata specified by the (name, name2, name3) tuple.
 *
 * Returns the a pointer to the userdata
 */
void *
xmlHashLookup3(xmlHashTablePtr table, const xmlChar *name,
	       const xmlChar *name2, const xmlChar *name3) {
    unsigned long key;
    xmlHashEntryPtr entry;

    if (table == NULL)
	return(NULL);
    if (name == NULL)
	return(NULL);
    key = xmlHashComputeKey(table, name);
    for (entry = table->table[key]; entry != NULL; entry = entry->next) {
	if ((xmlStrEqual(entry->name, name)) &&
	    (xmlStrEqual(entry->name2, name2)) &&
	    (xmlStrEqual(entry->name3, name3)))
	    return(entry->payload);
    }
    return(NULL);
}

/**
 * xmlHashScan:
 * @table: the hash table
 * @f:  the scanner function for items in the hash
 * @data:  extra data passed to f
 *
 * Scan the hash table and applied f to each value.
 */
void
xmlHashScan(xmlHashTablePtr table, xmlHashScanner f, void *data) {
    int i;
    xmlHashEntryPtr iter;
    xmlHashEntryPtr next;

    if (table == NULL)
	return;
    if (f == NULL)
	return;

    if (table->table) {
	for(i = 0; i < table->size; i++) {
	    iter = table->table[i];
	    while (iter) {
		next = iter->next;
		if (f)
		    f(iter->payload, data, iter->name);
		iter = next;
	    }
	}
    }
}

/**
 * xmlHashScan3:
 * @table: the hash table
 * @name: the name of the userdata or NULL
 * @name2: a second name of the userdata or NULL
 * @name3: a third name of the userdata or NULL
 * @f:  the scanner function for items in the hash
 * @data:  extra data passed to f
 *
 * Scan the hash table and applied f to each value matching
 * (name, name2, name3) tuple. If one of the names is null,
 * the comparison is considered to match.
 */
void
xmlHashScan3(xmlHashTablePtr table, const xmlChar *name,
	     const xmlChar *name2, const xmlChar *name3,
	     xmlHashScanner f, void *data) {
    int i;
    xmlHashEntryPtr iter;
    xmlHashEntryPtr next;

    if (table == NULL)
	return;
    if (f == NULL)
	return;

    if (table->table) {
	for(i = 0; i < table->size; i++) {
	    iter = table->table[i];
	    while (iter) {
		next = iter->next;
		if (((name == NULL) || (xmlStrEqual(name, iter->name))) &&
		    ((name2 == NULL) || (xmlStrEqual(name2, iter->name2))) &&
		    ((name3 == NULL) || (xmlStrEqual(name3, iter->name3)))) {
		    f(iter->payload, data, iter->name);
		}
		iter = next;
	    }
	}
    }
}

/**
 * xmlHashCopy:
 * @table: the hash table
 * @f:  the copier function for items in the hash
 *
 * Scan the hash table and applied f to each value.
 *
 * Returns the new table or NULL in case of error.
 */
xmlHashTablePtr
xmlHashCopy(xmlHashTablePtr table, xmlHashCopier f) {
    int i;
    xmlHashEntryPtr iter;
    xmlHashEntryPtr next;
    xmlHashTablePtr ret;

    if (table == NULL)
	return(NULL);
    if (f == NULL)
	return(NULL);

    ret = xmlHashCreate(table->size);
    if (table->table) {
	for(i = 0; i < table->size; i++) {
	    iter = table->table[i];
	    while (iter) {
		next = iter->next;
		xmlHashAddEntry3(ret, iter->name, iter->name2,
			         iter->name3, f(iter->payload, iter->name));
		iter = next;
	    }
	}
    }
    return(ret);
}

