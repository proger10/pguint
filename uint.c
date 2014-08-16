#include <postgres.h>
#include <fmgr.h>

#include <utils/array.h>
#include <utils/builtins.h>


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(uint4in);
PG_FUNCTION_INFO_V1(uint4out);
PG_FUNCTION_INFO_V1(uint4_avg_accum);
PG_FUNCTION_INFO_V1(int8_avg);
PG_FUNCTION_INFO_V1(uint4_sum);

Datum
uint4in(PG_FUNCTION_ARGS)
{
	char	   *s = PG_GETARG_CSTRING(0);
	unsigned long int result;

	if (s == NULL)
		elog(ERROR, "NULL pointer");
	if (*s == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for unsigned integer: \"%s\"",
						s)));

	if (strchr(s, '-'))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for unsigned integer: \"%s\"",
						s)));

	result = strtoul(s, NULL, 10);

	PG_RETURN_UINT32(result);
}

Datum
uint4out(PG_FUNCTION_ARGS)
{
	uint32		arg1 = PG_GETARG_UINT32(0);
	char	   *result = (char *) palloc(11);	/* 10 digits, '\0' */

	sprintf(result, "%u", arg1);
	PG_RETURN_CSTRING(result);
}

typedef struct Int8TransTypeData
{
	int64		count;
	int64		sum;
} Int8TransTypeData;

Datum
uint4_avg_accum(PG_FUNCTION_ARGS)
{
	ArrayType  *transarray;
	uint32		newval = PG_GETARG_UINT32(1);
	Int8TransTypeData *transdata;

	/*
	 * If we're invoked as an aggregate, we can cheat and modify our first
	 * parameter in-place to reduce palloc overhead. Otherwise we need to make
	 * a copy of it before scribbling on it.
	 */
	if (AggCheckCallContext(fcinfo, NULL))
		transarray = PG_GETARG_ARRAYTYPE_P(0);
	else
		transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);

	if (ARR_HASNULL(transarray) ||
		ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
		elog(ERROR, "expected 2-element int8 array");

	transdata = (Int8TransTypeData *) ARR_DATA_PTR(transarray);
	transdata->count++;
	transdata->sum += newval;

	PG_RETURN_ARRAYTYPE_P(transarray);
}

Datum
int8_avg(PG_FUNCTION_ARGS)
{
	ArrayType  *transarray = PG_GETARG_ARRAYTYPE_P(0);
	Int8TransTypeData *transdata;
	Datum		countd,
				sumd;

	if (ARR_HASNULL(transarray) ||
		ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
		elog(ERROR, "expected 2-element int8 array");
	transdata = (Int8TransTypeData *) ARR_DATA_PTR(transarray);

	/* SQL defines AVG of no values to be NULL */
	if (transdata->count == 0)
		PG_RETURN_NULL();

	countd = DirectFunctionCall1(int8_numeric,
								 Int64GetDatumFast(transdata->count));
	sumd = DirectFunctionCall1(int8_numeric,
							   Int64GetDatumFast(transdata->sum));

	PG_RETURN_DATUM(DirectFunctionCall2(numeric_div, sumd, countd));
}

Datum
uint4_sum(PG_FUNCTION_ARGS)
{
	int64		newval;

	if (PG_ARGISNULL(0))
	{
		/* No non-null input seen so far... */
		if (PG_ARGISNULL(1))
			PG_RETURN_NULL();   /* still no non-null */
		/* This is the first non-null input. */
		newval = (int64) PG_GETARG_UINT32(1);
		PG_RETURN_INT64(newval);
	}

	/*
	 * If we're invoked as an aggregate, we can cheat and modify our first
	 * parameter in-place to avoid palloc overhead. If not, we need to return
	 * the new value of the transition variable. (If int8 is pass-by-value,
	 * then of course this is useless as well as incorrect, so just ifdef it
	 * out.)
	 */
#ifndef USE_FLOAT8_BYVAL		/* controls int8 too */
	if (AggCheckCallContext(fcinfo, NULL))
	{
		int64	   *oldsum = (int64 *) PG_GETARG_POINTER(0);

		/* Leave the running sum unchanged in the new input is null */
		if (!PG_ARGISNULL(1))
			*oldsum = *oldsum + (int64) PG_GETARG_UINT32(1);

		PG_RETURN_POINTER(oldsum);
	}
	else
#endif
	{
		int64		oldsum = PG_GETARG_INT64(0);

		/* Leave sum unchanged if new input is null. */
		if (PG_ARGISNULL(1))
			PG_RETURN_INT64(oldsum);

		/* OK to do the addition. */
		newval = oldsum + (int64) PG_GETARG_UINT32(1);

		PG_RETURN_INT64(newval);
	}
}