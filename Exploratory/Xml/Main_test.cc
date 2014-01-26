#include "Prelude.hh"
#include "rapidxml.hpp"

using namespace ZZ;
using namespace rapidxml;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Array<char> text = readFile("CEA9601.xml", true);
    xml_document<char> doc;
    doc.parse<0>(&text[0]);

    WriteLn "First node: %_", doc.first_node()->name();

    return 0;
}
