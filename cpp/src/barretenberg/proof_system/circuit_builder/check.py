# TODO strs can be omitted
# TODO how exactly ref works
import cvc5
from cvc5 import Kind
import json
import sys
import time

p = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001

s = cvc5.Solver()
F = s.mkFiniteFieldSort(p)
s.setOption("produce-models", "true") # debug

def const_equal(var, t, s=s, F=F):
    tmp = s.mkFiniteFieldElem(t, F)
    return s.mkTerm(Kind.EQUAL, var, tmp)

def initiate_circuit(circuit_info, s=s, F=F):
    variables = circuit_info['variables']
    public_inps = circuit_info['public_inps']
    vars_of_interest = circuit_info['vars_of_interest']
    gates = circuit_info['gates']
    
    num_vars = len(variables)
    num_public_vars = len(public_inps)
    
    vars = [s.mkConst(F, vars_of_interest[i]) if i in vars_of_interest else s.mkConst(F, f"var_{i}") for i in range(num_vars)]
    

    s.assertFormula(const_equal(vars[0], 0))
    s.assertFormula(const_equal(vars[1], 1)) # CHECK THE REFERENCE

    for i in public_inps:
        s.assertFormula(const_equal(vars[i], variables[i]))
        #print(variables[i])
    return vars, vars_of_interest, gates

def add_gates(gates, vars, s=s, F=F):
    constrs = []
    for sel_wit in gates:
        #sel_wit = [x if x < p // 2 else x - p for x in sel_wit] # % p some time
        q_m, q_1, q_2, q_3, q_c, w_l, w_r, w_o = sel_wit
        eq = s.mkFiniteFieldElem(0, F)
        if q_m != 0:
            tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, vars[w_l], vars[w_r])
            q_m = s.mkFiniteFieldElem(q_m, F)
            tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, tmp, q_m)
            eq = s.mkTerm(Kind.FINITE_FIELD_ADD, eq, tmp)
        if q_1 != 0:
            q_1 = s.mkFiniteFieldElem(q_1, F)
            tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, vars[w_l], q_1)
            eq = s.mkTerm(Kind.FINITE_FIELD_ADD, eq, tmp)
        if q_2 != 0:
            q_2 = s.mkFiniteFieldElem(q_2, F)
            tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, vars[w_r], q_2)
            eq = s.mkTerm(Kind.FINITE_FIELD_ADD, eq, tmp)
        if q_3 != 0:
            q_3 = s.mkFiniteFieldElem(q_3, F)
            tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, vars[w_o], q_3)
            eq = s.mkTerm(Kind.FINITE_FIELD_ADD, eq, tmp)
        if q_c != 0:
            q_c = s.mkFiniteFieldElem(q_c, F)
            eq = s.mkTerm(Kind.FINITE_FIELD_ADD, eq, q_c)
        #print(eq)
        constrs.append(s.mkTerm(Kind.EQUAL, eq, s.mkFiniteFieldElem(0, F)))
    return constrs

fname = sys.argv[1]
circuit_info = eval(open(fname).read()) # bruh #2

vars, vars_of_interest, gates = initiate_circuit(circuit_info, s)

inputs = [vars[i] for i in range(len(vars)) if i in vars_of_interest]
print(inputs)
print(vars_of_interest)

def poly(inputs, s=s, F=F):
    coeffs = inputs[:-2]
    point = inputs[-2]
    result = inputs[-1]
    ev = s.mkFiniteFieldElem("0", F)
    for i in range(len(coeffs)):
        tmp = s.mkTerm(Kind.FINITE_FIELD_MULT, ev, point)
        ev = s.mkTerm(Kind.FINITE_FIELD_ADD, tmp, coeffs[i])

    res = s.mkTerm(Kind.EQUAL, ev, result)
    ret = s.mkTerm(Kind.EQUAL, res, s.mkBoolean(0))
#    print(ret)
    return ret#, ev


polynomial = poly(inputs)
s.assertFormula(polynomial)

gts = add_gates(gates, vars)
for g in gts:
    s.assertFormula(g)

start = time.time()
res = s.checkSat()
end = time.time()
print(f"Time elapsed: {end - start}, gates: {len(gates)}")

print(res)
if res.isSat():
    for x in inputs:
        print(x, "=", s.getValue(x).toPythonObj())
    
#    print(s.getValue(ev).toPythonObj())