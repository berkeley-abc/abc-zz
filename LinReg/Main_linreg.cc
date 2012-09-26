#include "Prelude.hh"
#include "LinReg.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Vec<Vec<double> > data;     // -- 'data[var#][sample#]'

    // Curve 0:
    data(0)(0) = 0;
    data(1)(0) = 0;
    data(2)(0) = 0;
    data(3)(0) = 1;
    data(4)(0) = 2;

    data(0)(1) = 0;
    data(1)(1) = 2;
    data(2)(1) = 0;
    data(3)(1) = 1;
    data(4)(1) = 3;

    data(0)(2) = 0;
    data(1)(2) = 4;
    data(2)(2) = 0;
    data(3)(2) = 1;
    data(4)(2) = 5;

    // Curve 1:
    data(0)(3) = 1;
    data(1)(3) = 0;
    data(2)(3) = 0;
    data(3)(3) = 1;
    data(4)(3) = 1;

    data(0)(4) = 1;
    data(1)(4) = 2;
    data(2)(4) = 2;
    data(3)(4) = 1;
    data(4)(4) = 0;

    data(0)(5) = 1;
    data(1)(5) = 4;
    data(2)(5) = 4;
    data(3)(5) = 1;
    data(4)(5) = 1;


    Vec<double> coeff;
    bool stable = linearRegression(data, coeff);

    Dump(stable);
    Dump(coeff);

    WriteLn "%_", 0*coeff[0] + 0*coeff[1] + 0*coeff[2] + coeff[3];
    WriteLn "%_", 0*coeff[0] + 2*coeff[1] + 0*coeff[2] + coeff[3];
    WriteLn "%_", 0*coeff[0] + 4*coeff[1] + 0*coeff[2] + coeff[3];
    NewLine;

    WriteLn "%_", 1*coeff[0] + 0*coeff[1] + 0*coeff[2] + coeff[3];
    WriteLn "%_", 1*coeff[0] + 2*coeff[1] + 2*coeff[2] + coeff[3];
    WriteLn "%_", 1*coeff[0] + 4*coeff[1] + 4*coeff[2] + coeff[3];

    return 0;
}
