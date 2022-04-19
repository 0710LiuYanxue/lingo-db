--//RUN: run-sql %s %S/../../../resources/data/tpch | FileCheck %s
--//CHECK: |                   supp_nation  |                   cust_nation  |                        l_year  |                       revenue  |
--//CHECK: -------------------------------------------------------------------------------------------------------------------------------------
--//CHECK: |                      "FRANCE"  |                     "GERMANY"  |                          1995  |                  4637235.1501  |
--//CHECK: |                      "FRANCE"  |                     "GERMANY"  |                          1996  |                  5224779.5736  |
--//CHECK: |                     "GERMANY"  |                      "FRANCE"  |                          1995  |                  6232818.7037  |
--//CHECK: |                     "GERMANY"  |                      "FRANCE"  |                          1996  |                  5557312.1121  |
-- TPC-H Query 7

select
        supp_nation,
        cust_nation,
        l_year,
        sum(volume) as revenue
from
        (
                select
                        n1.n_name as supp_nation,
                        n2.n_name as cust_nation,
                        extract(year from l_shipdate) as l_year,
                        l_extendedprice * (1 - l_discount) as volume
                from
                        supplier,
                        lineitem,
                        orders,
                        customer,
                        nation n1,
                        nation n2
                where
                        s_suppkey = l_suppkey
                        and o_orderkey = l_orderkey
                        and c_custkey = o_custkey
                        and s_nationkey = n1.n_nationkey
                        and c_nationkey = n2.n_nationkey
                        and (
                                (n1.n_name = 'FRANCE' and n2.n_name = 'GERMANY')
                                or (n1.n_name = 'GERMANY' and n2.n_name = 'FRANCE')
                        )
                        and l_shipdate between date '1995-01-01' and date '1996-12-31'
        ) as shipping
group by
        supp_nation,
        cust_nation,
        l_year
order by
        supp_nation,
        cust_nation,
        l_year

