#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "afdb.h"
#include "dynam.h"

const AFDB_Version AFDB_CurrentVersion = {
	.Major = 0,
	.Minor = 0,
	.Patch = 0,
};

void AFDB_Info(const AFDB_TableHeader Header)
{
	fprintf(stderr, " [INFO] Row count %ld\n", Header.Rows);
	fprintf(stderr, " [INFO] Column count %d\n",
			Header.Columns);
	fprintf(stderr, " [INFO] Next Table %ld\n",
			Header.NextTable);
}

void AFDB_CleanTable(AFDB_Table *Table)
{
	if (!Table)
		return;
	fprintf(stderr, " [INFO] Cleaning Table with %d Columns at %p\n", Table->Header.Columns, (void *)Table);
	for (size_t i = 0L; i < Table->Header.Columns; ++i)
	{
		fprintf(stderr,
			" [INFO] Attemping clean on column %ld (%.32s) (%d)\n",
			i, Table->Columns[i].Raw.Name, Table->Columns[i].Raw.Type);
	}

	for (size_t i = 0L; i < Table->Header.Rows; ++i)
	{
		fprintf(stderr,
			" [INFO] Attemping clean on row %ld\n", i);
		for (size_t j = 0L; j < Table->Header.Columns; ++j)
		{
			if (Table->Columns[j].Raw.Type == AFDB_VARCHAR &&
		    	    Table->Rows[i].items[j].as.VarChar.p)
			{
				free(Table->Rows[i].items[j].as.VarChar.p);
			}
		}
	}

	free(Table->Columns);
	free(Table->Rows->items);
	free(Table->Rows);
	memset(Table, 0, sizeof(*Table));
}

void AFDB_CleanTables(AFDB_Table *Tables, const size_t Count)
{
	if (!Tables)
		return;
	fprintf(stderr, " [INFO] Cleaning %ld Tables at %p\n", Count, (void *)Tables);
	for (size_t i = 0L; i < Count; ++i)
	{
		AFDB_CleanTable(&Tables[i]);
	}
}

AFDB_DB AFDB_Load(const char *const path)
{
	if (!path)
	{
		fprintf(stderr, " [ERROR] Null path provided\n");
		return (AFDB_DB){0};
	}
	FILE *fp = fopen(path, "rb");
	if (!fp)
	{
		fprintf(stderr, " [ERROR] File not found\n");
		return (AFDB_DB){0};
	}

	/* Grab Size */
	fseek(fp, 0, SEEK_END);
	const size_t FileLength = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Grab Header */
	AFDB_Header Header = {0};
	fread(&Header, 1, sizeof(Header), fp);

	/* Do initial checks */
	if (strncmp(Header.Magic, DBMagic, 5) ||
	    AFDB_CurrentVersion.Major < Header.Version.Major ||
	    AFDB_CurrentVersion.Minor < Header.Version.Minor ||
	    AFDB_CurrentVersion.Patch < Header.Version.Patch)
	{
		fprintf(stderr, " [ERROR] Invalid file or incorrect version\n");
		fclose(fp); fp = NULL;
		return (AFDB_DB){0};
	}

	if (Header.TableCount == 0L)
	{
		fprintf(stderr, " [ERROR] No tables present\n");
		fclose(fp); fp = NULL;
		return (AFDB_DB){0};
	}

	/* Find Tables */

	/**
	 * Owned by caller
	 */
	AFDB_Table *Tables = calloc(Header.TableCount,
				    sizeof(*Tables));
	if (!Tables)
	{
		fprintf(stderr, " [ERROR] Could not allocate tables\n");
		fclose(fp); fp = NULL;
		return (AFDB_DB){0};
	}

	size_t current_off = Header.FirstTable;

	for (size_t i = 0L; i < Header.TableCount; ++i)
	{
		if (current_off + sizeof(Header) >= FileLength)
		{
			fprintf(stderr, " [ERROR] Offset is out of bounds in file\n");
			fclose(fp); fp = NULL;
			AFDB_CleanTables(Tables, Header.TableCount);
			free(Tables); Tables = NULL;
			return (AFDB_DB){0};
		}
		fprintf(stderr, " [INFO] Searching for table at offset %ld (%ld)\n",
			current_off + sizeof(Header), current_off);
		const AFDB_TableHeader TableHeader;
		fseek(fp, current_off + sizeof(Header), SEEK_SET);
		fread((AFDB_TableHeader *)&TableHeader, 1, sizeof(TableHeader), fp);

		/**
		 * Sanity check 01
		 * */
		if (strncmp(TableHeader.Magic, TBMagic, 5))
		{
			fprintf(stderr, " [ERROR] Invalid table at file offset %ld (%ld)\n",
				current_off + sizeof(Header), current_off);
			fclose(fp); fp = NULL;
			AFDB_CleanTables(Tables, Header.TableCount);
			free(Tables); Tables = NULL;
			return (AFDB_DB){0};
		}

		/*
		 * Owned by caller
		 */
		AFDB_Column *Columns = calloc(TableHeader.Columns,
					      sizeof(*Columns));
		if (!Columns)
		{
			fprintf(stderr, " [ERROR] Could not allocate columns\n");
			fclose(fp); fp = NULL;
			AFDB_CleanTables(Tables, Header.TableCount);
			free(Tables); Tables = NULL;
			return (AFDB_DB){0};
		}

		/* Load Columns */
		for (size_t j = 0L; j < TableHeader.Columns; ++j)
		{
			AFDB_Column Column = {0};
			fread(&Column.Raw, 1, sizeof(Column.Raw), fp);
			fprintf(stderr, " [INFO] Loading column %s (%ld)\n", Column.Raw.Name, j);
			if (Column.Raw.Type == AFDB_VARCHAR)
			{
				/**
				 * Example for loading a varchar into a row
				Column.Data = calloc(Column.Raw.as.VarChar_Length + 1, sizeof(char));
				if (!Column.Data)
				{
					fprintf(stderr, " [ERROR] Could not allocate column\n");
					fclose(fp); fp = NULL;
					AFDB_CleanTables(Tables, Header.TableCount);
					free(Tables); Tables = NULL;
					memset(Columns, 0, sizeof(*Columns) * TableHeader.Columns);
					free(Columns);
					return (AFDB_DB){0};
				}

				fread(Column.Data, 1, Column.Raw.as.VarChar_Length, fp);
				((char *)Column.Data)[Column.Raw.as.VarChar_Length] = 0;
				*/
			}

			memcpy(&Columns[j], &Column, sizeof(Column));
		}

		/*
		 * Owned by caller
		 */
		AFDB_Row *Rows = calloc(TableHeader.Rows,
					sizeof(*Rows));
		if (!Rows)
		{
			fprintf(stderr, " [ERROR] Could not allocate rows\n");
			fclose(fp); fp = NULL;
			AFDB_CleanTables(Tables, Header.TableCount);
			free(Tables); Tables = NULL;
			free(Columns); Columns = NULL;
			return (AFDB_DB){0};
		}

		/* Load Rows */
		for (size_t j = 0L; j < TableHeader.Rows; ++j)
		{
			AFDB_Row Row = {0};
			for (size_t col = 0L; col < TableHeader.Columns; ++col)
			{
				AFDB_Item Item = {0};
				switch (Columns[col].Raw.Type)
				{
				case AFDB_INTEGER:
					fread(&Item.as.Integer, 1, sizeof(long long), fp);
					break;
				case AFDB_VARCHAR:
					{
					fread(&Item.as.VarChar.Length, 1, sizeof(long long), fp);
					Item.as.VarChar.p = calloc(Item.as.VarChar.Length + 1, sizeof(char));
					if (!Item.as.VarChar.p)
					{
						fprintf(stderr, " [ERROR] Could not allocate VarChar(%lld) row\n",
							Item.as.VarChar.Length);
						fclose(fp); fp = NULL;
						AFDB_CleanTables(Tables, Header.TableCount);
						free(Tables); Tables = NULL;
						free(Columns); Columns = NULL;
						free(Rows); Rows = NULL;
						return (AFDB_DB){0};
					}

					fread(Item.as.VarChar.p, 1, Item.as.VarChar.Length, fp);
					Item.as.VarChar.p[Item.as.VarChar.Length] = '\0';
					break;
					}
				default:
					break;
				}

				/* TODO - Make this allocate
				 * all at once over per iteration */
				da_append(&Row, Item);
			}
			memcpy(&Rows[j], &Row, sizeof(Row));
		}

		const AFDB_Table Table = {
			.Header = TableHeader,
			.Columns = Columns, /*TODO - Read columns, should come straight after */
			.Rows = Rows,
		};

		AFDB_Info(TableHeader);

		current_off = TableHeader.NextTable;
		memcpy(&Tables[i], &Table, sizeof(Table));
	}

	fclose(fp); fp = NULL;
	return (AFDB_DB){.Header = Header,
			 .items = Tables,
			 .count = Header.TableCount,
			 .capacity = Header.TableCount};
}

void AFDB_WriteTable(FILE *fp, AFDB_Table *Table, size_t NextOffset)
{
	if (!fp || !Table)
		return;
	AFDB_TableHeader Header = {
		.Magic = TBMagic,
		.Rows = Table->Header.Rows,
		.Columns = Table->Header.Columns,
		.NextTable = NextOffset,
	};

	fwrite(&Header, 1, sizeof(Header), fp);

	for (size_t col = 0L; col < Table->Header.Columns; ++col)
	{
		AFDB_RawColumn *Column = &Table->Columns[col].Raw;
		fwrite(Column, 1, sizeof(*Column), fp);
	}

	for (size_t row = 0L; row < Table->Header.Rows; ++row)
	{
		AFDB_Row *Row = &Table->Rows[row];
		for (size_t c = 0L; c < Table->Header.Columns; ++c)
		{
		switch (Table->Columns[c].Raw.Type)
		{
			case AFDB_INTEGER:
				fwrite(&Row->items[c].as.Integer, 1, sizeof(long long), fp);
				break;
			case AFDB_VARCHAR:
				fwrite(&Row->items[c].as.VarChar.Length, 1, sizeof(long long), fp);
				fwrite(Row->items[c].as.VarChar.p, Row->items[c].as.VarChar.Length, sizeof(char), fp);
				break;
			default:
				break;
		}
		}
	}
}

void AFDB_Write(const char *const path, AFDB_DB *Db)
{
	if (!path || !Db || !Db->items)
		return;
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	/* Construct Header */
	AFDB_Header Header = {
		.Magic = DBMagic,
		.Version = AFDB_CurrentVersion,
		.TableCount = Db->count,
		.FirstTable = 0,
	};

	fwrite(&Header, 1, sizeof(Header), fp);

	size_t CurrentOffset = 0, PrevOffset = 0;
	for (size_t i = 0L; i < Db->count; ++i)
	{
		AFDB_WriteTable(fp, &Db->items[i], PrevOffset);
		PrevOffset = CurrentOffset;
		CurrentOffset = ftell(fp) - sizeof(Header);
	}

	fseek(fp, (char *)&Header.FirstTable - (char *)&Header, SEEK_SET);
	fwrite(&PrevOffset, sizeof(PrevOffset), 1, fp);

	fclose(fp); fp = NULL;
}

int main(int arg_c, char **arg_v)
{
	if (arg_c != 2)
		return 1;
	AFDB_DB db = AFDB_Load(arg_v[1]);
	if (db.items)
	{
		fprintf(stderr, " [INFO] %ld tables\n", db.count);
		for (size_t i = 0L; i < db.count; ++i)
		{
			/* TODO - Rows */
			fprintf(stderr, " [INFO] table %ld of %ld rows and %d columns\n",
				i, db.items[i].Header.Rows, db.items[i].Header.Columns);
			const char *const TypeNames[] = {
				"Null", "Integer", "VarChar"
			};
			for (size_t j = 0L; j < db.items[i].Header.Columns; ++j)
			{
				fprintf(stderr, "\t%s:%s\t",
					db.items[i].Columns[j].Raw.Name,
					TypeNames[db.items[i].Columns[j].Raw.Type]);	
			}

			fprintf(stderr, "\n");

			for (size_t j = 0L; j < db.items[i].Header.Rows; ++j)
			{
				for (size_t k = 0L; k < db.items[i].Header.Columns; ++k)
				{
					AFDB_Item *Item = &db.items[i].Rows[j].items[k];
					AFDB_Column *Col = &db.items[i].Columns[k];
					if (Col->Raw.Type == AFDB_NULL)
						fprintf(stderr, "\tNULL\t");
					else 
					if (Col->Raw.Type == AFDB_INTEGER)
						fprintf(stderr, "\t%lld\t", Item->as.Integer);
					else 
					if (Col->Raw.Type == AFDB_VARCHAR)
						fprintf(stderr, "\t%s\t", Item->as.VarChar.p);
				}

				fprintf(stderr, "\n");
			}
		}
		
		AFDB_Write("~tmp.fdb", &db);
		AFDB_CleanTables(db.items, db.count);
		free(db.items);
	}
	else
	{
		fprintf(stderr, " [ERROR] Could not get db\n");
	}

	return 0;
}

