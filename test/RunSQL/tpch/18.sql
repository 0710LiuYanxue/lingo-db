--//RUN: run-sql %s %S/../../../resources/data/tpch | FileCheck %s
--//CHECK: |                        c_name  |                     c_custkey  |                    o_orderkey  |                   o_orderdate  |                  o_totalprice  |                           sum  |
--//CHECK: -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
--//CHECK: |          "Customer#000001639"  |                          1639  |                        502886  |                    1994-04-12  |                     456423.88  |                        312.00  |
--//CHECK: |          "Customer#000006655"  |                          6655  |                         29158  |                    1995-10-21  |                     452805.02  |                        305.00  |
--//CHECK: |          "Customer#000014110"  |                         14110  |                        565574  |                    1995-09-24  |                     425099.85  |                        301.00  |
--//CHECK: |          "Customer#000001775"  |                          1775  |                          6882  |                    1997-04-09  |                     408368.10  |                        303.00  |
--//CHECK: |          "Customer#000011459"  |                         11459  |                        551136  |                    1993-05-19  |                     386812.74  |                        308.00  |
-- TPC-H Query 18

select
        c_name,
        c_custkey,
        o_orderkey,
        o_orderdate,
        o_totalprice,
        sum(l_quantity)
from
        customer,
        orders,
        lineitem
where
        o_orderkey in (
                select
                        l_orderkey
                from
                        lineitem
                group by
                        l_orderkey having
                                sum(l_quantity) > 300
        )
        and c_custkey = o_custkey
        and o_orderkey = l_orderkey
group by
        c_name,
        c_custkey,
        o_orderkey,
        o_orderdate,
        o_totalprice
order by
        o_totalprice desc,
        o_orderdate
limit 100

