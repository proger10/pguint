new_types = ['int1', 'uint1', 'uint2', 'uint4', 'uint8']
old_types = ['int2', 'int4', 'int8']

op_words = {'<': 'lt', '<=': 'le', '=': 'eq', '<>': 'ne', '>=': 'ge', '>': 'gt',
            '+': 'pl', '-': 'mi', '*': 'mul', '/': 'div', '%': 'mod'}

c_types = {
    'boolean': 'bool',
    'int1': 'int8',
    'int2': 'int16',
    'int4': 'int32',
    'int8': 'int64',
    'uint1': 'uint8',
    'uint2': 'uint16',
    'uint4': 'uint32',
    'uint8': 'uint64',
}

f_operators_c = open('operators.c', 'w')
f_operators_sql = open('operators.sql', 'w')
f_test_operators_sql = open('test/sql/operators.sql', 'w')

f_operators_c.write('#include <postgres.h>\n')
f_operators_c.write('#include <fmgr.h>\n\n')
f_operators_c.write('#include "uint.h"\n\n')


def write_c_function(f, funcname, leftarg, rightarg, op, rettype):
    f.write("""
PG_FUNCTION_INFO_V1(%s);
Datum
%s(PG_FUNCTION_ARGS)
{
	%s arg1 = PG_GETARG_%s(0);
	%s arg2 = PG_GETARG_%s(1);

	PG_RETURN_%s(arg1 %s arg2);
}
""" % (funcname, funcname,
       c_types[leftarg], c_types[leftarg].upper(),
       c_types[rightarg], c_types[rightarg].upper(),
       c_types[rettype].upper(),
       op if op != '<>' else '!='))


def write_sql_operator(f, funcname, leftarg, rightarg, op, rettype):
    f.write("""
CREATE FUNCTION %s(%s, %s) RETURNS %s IMMUTABLE LANGUAGE C AS '$libdir/uint', '%s';

CREATE OPERATOR %s (
    PROCEDURE = %s,
    LEFTARG = %s,
    RIGHTARG = %s
);
""" % (funcname, leftarg, rightarg, rettype, funcname,
       op, funcname, leftarg, rightarg))


def write_code(f_c, f_sql, leftarg, rightarg, op, rettype):
    funcname = leftarg + rightarg + op_words[op]
    write_c_function(f_c, funcname, leftarg, rightarg, op, rettype)
    write_sql_operator(f_sql, funcname, leftarg, rightarg, op, rettype)


for leftarg in new_types + old_types:
    for rightarg in new_types + old_types:
        if leftarg in old_types and rightarg in old_types:
            continue
        for op in ['<', '<=', '=', '<>', '>=', '>']:
            write_code(f_operators_c, f_operators_sql, leftarg, rightarg, op, rettype='boolean')
            f_test_operators_sql.write("SELECT '1'::%s %s '1'::%s;\n" % (leftarg, op, rightarg))
            f_test_operators_sql.write("SELECT '5'::%s %s '2'::%s;\n" % (leftarg, op, rightarg))
            f_test_operators_sql.write("SELECT '3'::%s %s '4'::%s;\n" % (leftarg, op, rightarg))
        f_test_operators_sql.write("\n")

        for op in ['+', '-', '*', '/', '%']:
            rettype = max(leftarg, rightarg)
            write_code(f_operators_c, f_operators_sql, leftarg, rightarg, op, rettype)
            f_test_operators_sql.write("SELECT pg_typeof('1'::%s %s '1'::%s);\n" % (leftarg, op, rightarg))
            f_test_operators_sql.write("SELECT '1'::%s %s '1'::%s;\n" % (leftarg, op, rightarg))
            f_test_operators_sql.write("SELECT '3'::%s %s '4'::%s;\n" % (leftarg, op, rightarg))
            f_test_operators_sql.write("SELECT '5'::%s %s '2'::%s;\n" % (leftarg, op, rightarg))
        f_test_operators_sql.write("\n")

f_operators_c.close()
f_operators_sql.close()
f_test_operators_sql.close()
