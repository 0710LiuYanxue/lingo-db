--//RUN: run-sql %s %S/../../../../resources/data/tpch | FileCheck %s
--//CHECK: |                        s_name  |                     s_address  |
--//CHECK: -------------------------------------------------------------------
--//CHECK: |          "Supplier#000000157"  |                 ",mEGorBfVIm"  |
--//CHECK: |          "Supplier#000000197"  |"YC2Acon6kjY3zj3Fbxs2k4Vdf7X0cd2F"  |
--//CHECK: |          "Supplier#000000287"  |           "7a9SP7qW5Yku5PvSg"  |
--//CHECK: |          "Supplier#000000378"  |          "FfbhyCxWvcPrO8ltp9"  |
--//CHECK: |          "Supplier#000000530"  |"0qwCMwobKY OcmLyfRXlagA8ukENJv,"  |
--//CHECK: |          "Supplier#000000555"  |"TfB,a5bfl3Ah 3Z 74GqnNs6zKVGM"  |
--//CHECK: |          "Supplier#000000557"  |    "jj0wUYh9K3fG5Jhdhrkuy ,4"  |
--//CHECK: |          "Supplier#000000729"  |"pqck2ppy758TQpZCUAjPvlU55K3QjfL7Bi"  |
--//CHECK: |          "Supplier#000000935"  |"ij98czM 2KzWe7dDTOxB8sq0UfCdvrX"  |
-- TPC-H Query 20

select
        s_name,
        s_address
from
        supplier,
        nation
where
        s_suppkey in (
                select
                        ps_suppkey
                from
                        partsupp
                where
                        ps_partkey in (
                                select
                                        p_partkey
                                from
                                        part
                                where
                                        p_name like 'forest%'
                        )
                        and ps_availqty > (
                                select
                                        0.5 * sum(l_quantity)
                                from
                                        lineitem
                                where
                                        l_partkey = ps_partkey
                                        and l_suppkey = ps_suppkey
                                        and l_shipdate >= date '1994-01-01'
                                        and l_shipdate < date '1995-01-01'
                        )
        )
        and s_nationkey = n_nationkey
        and n_name = 'CANADA'
order by
        s_name

