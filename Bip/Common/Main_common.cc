#include "Prelude.hh"
#include "Ltl.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Netlist N;
    String msg;
    Wire w = parseLtl("a xnor b <-> c", N, msg);
    if (!w)
        WriteLn "LTL PARSE ERROR! %_", msg;
    else{
        WriteLn "NETLIST:";
        N.write(std_out);
        NewLine;
        WriteLn "Parsed top-node: %n", w;
    }

    return 0;
}


/*
EXAMPLES

(F(G(node[1].root)))->(!((node[1].state!=T3_root_2)S(node[0].state=T3_root_2)))
!((F(G(node[1].root)))->(!((node[1].state!=T3_root_2)S(node[0].state=T3_root_2))))
F((in_state_S0_start)|((timeout)|(known_problems)))
!(F((in_state_S0_start)|((timeout)|(known_problems))))
G((O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O(!(node[0].state=T3_root_1)))))))))->(F(G(X(!(node[0].state=T3_root_1))))))
G((O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O(!(node[0].state=T3_root_1)))))))))))))->(F(G(X(!(node[0].state=T3_root_1))))))
G((O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O((!(node[0].state=T3_root_1))&(O((node[0].state=T3_root_1)&(O(!(node[0].state=T3_root_1)))))))))))))))))->(F(G(X(!(node[0].state=T3_root_1))))))
(G(node[0].port[0].role=off))|((node[0].port[0].role=unknown)U(G((node[0].port[0].role=parent)|(node[0].port[0].role=child))))
!((G(node[0].port[0].role=off))|((node[0].port[0].role=unknown)U(G((node[0].port[0].role=parent)|(node[0].port[0].role=child)))))
(node[0].state=T0_start)&(X((node[0].state=T0_start)U((node[0].state=T1_child)&(X((node[0].state=T1_child)U((node[0].state=T2_parent)&(X((node[0].state=T2_parent)U(((node[0].state=T1_child)|((node[0].state=T2_parent)|((node[0].state=T3_root_1)|((node[0].state=T3_root_2)|(node[0].state=T3_root_3)))))U(G(node[0].state=S0_start)))))))))))
!((node[0].state=T0_start)&(X((node[0].state=T0_start)U((node[0].state=T1_child)&(X((node[0].state=T1_child)U((node[0].state=T2_parent)&(X((node[0].state=T2_parent)U(((node[0].state=T1_child)|((node[0].state=T2_parent)|((node[0].state=T3_root_1)|((node[0].state=T3_root_2)|(node[0].state=T3_root_3)))))U(G(node[0].state=S0_start))))))))))))
*/
