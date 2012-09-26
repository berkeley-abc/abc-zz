#include "Prelude.hh"
#include "Png.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Bitmap bm(800, 600, Color(0, 0, 200));
    fill(bm, 50, 50, 150, 150, Color(200, 0, 0));

    bm.flip();
    writePng("bm.png", bm);

    Bitmap bm2;
    bm2.slice(bm, 25, 25, 400, 400);
    writePng("bm2.png", bm2);

    return 0;
}
