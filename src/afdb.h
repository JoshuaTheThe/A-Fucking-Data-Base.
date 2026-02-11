#ifndef _AFDB_H
#define _AFDB_H

/**
 * The magic sequence inside the Header of the database file.
 * <offsets 0...4> (Includes Null termination)
 */
#define DBMagic "afdb"

/**
 * The magic sequence inside the Header of a table
 * <offsets 0...4> (Includes Null termination)
 */
#define TBMagic "tabl"

#define packed __attribute__((__packed__))

#define COLUMN_MAX_NAME (32)

/**
 * DataBase In File
 * */
typedef struct packed
{
	const char Major;
	const char Minor;
	const char Patch;
} packed AFDB_Version;

typedef struct packed
{
	const char Magic[5]; /* afdb\0 */
	const AFDB_Version Version;
	const size_t TableCount;

	/* Linked List offset from Header */
	const size_t FirstTable;
} packed AFDB_Header;

/**
 * Table In File
 * */

typedef enum
{
	AFDB_NULL,
	AFDB_INTEGER,
	AFDB_VARCHAR,
} AFDB_ColumnType;

typedef struct packed
{
	const AFDB_ColumnType Type;
	const char Name[COLUMN_MAX_NAME];

	/**
	 * There is no need to store the pointer to
	 * the next column, for they are read sequentially.
	 * when we are finished, we simply start from current_pos
	 * */
} packed AFDB_RawColumn;

typedef struct packed
{
	const char Magic[5]; /* tabl\0 */
	const size_t Rows;
	const unsigned short Columns;

	/* Linked List Offset from end of header */
	const size_t NextTable;

	/* Raw Columns Follow */
} packed AFDB_TableHeader;

/**
 * Table In Memory
 * */
typedef struct packed
{
	AFDB_RawColumn Raw;
} packed AFDB_Column;

typedef struct packed
{
	union
	{
		long long Integer;
		struct
		{
			char *p;
			long long Length;
		} VarChar;
	} as;
} packed AFDB_Item;

typedef struct packed
{
	AFDB_Item *items;
	size_t     count, capacity;
} packed AFDB_Row;

typedef struct packed
{
	AFDB_TableHeader Header; /* We ignore NextTable once loaded ofc */
	AFDB_Column *Columns;
	AFDB_Row    *Rows;
} packed AFDB_Table;

/**
 * DataBase In Memory
 * */
typedef struct packed
{
	AFDB_Header Header;
	AFDB_Table *items;
	size_t      count,capacity;
} packed AFDB_DB;

extern const AFDB_Version AFDB_CurrentVersion;

#endif

