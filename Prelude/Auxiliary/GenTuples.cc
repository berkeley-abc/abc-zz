//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GenTuples.cc
//| Author(s)   : Niklas Een
//| Module      : Auxiliary
//| Description : Code generator for the tuple classes ('Pair', 'Trip' and 'Quad').
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include <cstdio>
#include <cstring>
#include <cassert>
using namespace std;

//#define OLD_STYLE


//=================================================================================================


#if defined(OLD_STYLE)
  const char* field_names[] = { "x", "y", "z", "w" };
  const char* type_names [] = { "X", "Y", "Z", "W" };
#else
  const char* field_names[] = { "fst", "snd", "trd", "fth" };
  const char* type_names [] = { "Fst", "Snd", "Trd", "Fth" };
#endif


//=================================================================================================


void gen(const char* class_name, const char* factory_name, int size)
{
    printf("//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm\n");
    printf("// %s:\n", class_name);
    printf("\n");
    printf("\n");

    // Class definition:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s_", (i==0)?"":", ", type_names[i]);
    printf(">\n");

    printf("struct %s {\n", class_name);
    for (int i = 0; i < size; i++)
        printf("    typedef %s_ %s;\n", type_names[i], type_names[i]);
    printf("    enum { size = %d };\n", size);
    printf("\n");

    for (int i = 0; i < size; i++)
        printf("    %s %s;\n", type_names[i], field_names[i]);
    printf("\n");

    printf("    %s() {}\n", class_name);
    printf("    %s(", class_name);
    for (int i = 0; i < size; i++)
        printf("%sconst %s& %s_", (i==0)?"":", ", type_names[i], field_names[i]);
    printf(") : ");
    for (int i = 0; i < size; i++)
        printf("%s%s(%s_)", (i==0)?"":", ", field_names[i], field_names[i]);
    printf(" {}\n");
    printf("\n");

    printf("    template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %sCompat", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("    %s(const %s<", class_name, class_name);
    for (int i = 0; i < size; i++)
        printf("%s%sCompat", (i==0)?"":", ", type_names[i]);
    printf(">& tuple) : ");
    for (int i = 0; i < size; i++)
        printf("%s%s(tuple.%s)", (i==0)?"":", ", field_names[i], field_names[i]);
    printf(" {}\n");
    printf("\n");

    printf("    void split(");
    for (int i = 0; i < size; i++)
        printf("%s%s& out_%s", (i==0)?"":", ", type_names[i], field_names[i]);
    printf(") { ");
    for (int i = 0; i < size; i++)
        printf("out_%s = %s; ", field_names[i], field_names[i]);
    printf("}\n");
    printf("\n");

    printf("};\n\n\n");


    // Operators:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("inline bool operator==(");
    for (int i = 0; i < 2; i++){
        printf("%sconst %s<", (i==0)?"":", ", class_name);
        for (int j = 0; j < size; j++)
            printf("%s%s", (j==0)?"":", ", type_names[j]);
        printf(">& v%d", i);
    }
    printf(") {\n");
    printf("    return ");
    for (int i = 0; i < size; i++)
        printf("%s(v0.%s == v1.%s)", (i==0)?"":" && ", field_names[i], field_names[i]);
    printf("; }\n");
    printf("\n");

    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("inline bool operator<(");
    for (int i = 0; i < 2; i++){
        printf("%sconst %s<", (i==0)?"":", ", class_name);
        for (int j = 0; j < size; j++)
            printf("%s%s", (j==0)?"":", ", type_names[j]);
        printf(">& v%d", i);
    }
    printf(") {\n");
    printf("    return v0.%s < v1.%s", field_names[0], field_names[0]);
    for (int i = 1; i < size; i++)
        printf(" || (!(v1.%s < v0.%s) && (v0.%s < v1.%s", field_names[i-1], field_names[i-1], field_names[i], field_names[i]);
    for (int i = 0; i < size-1; i++)
        printf("))");
    printf("; }\n");
    printf("\n");

    // Factory function:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("inline %s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf("> %s(", factory_name);
    for (int i = 0; i < size; i++)
        printf("%sconst %s& %s", (i==0)?"":", ", type_names[i], field_names[i]);
    printf(") {\n");
    printf("    return %s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(">(");
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", field_names[i]);
    printf("); }\n");
    printf("\n\n");

    // Hash class:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("struct Hash_default<%s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf("> > {\n");

    printf("    uint64 hash(const %s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(" >& v) const {\n");
    for (int i = 0; i < size; i++)
        printf("        uint64 h%s = defaultHash(v.%s);\n", field_names[i], field_names[i]);
    printf("        return h%s", field_names[0]);
    for (int i = 1; i < size; i++){
        int shift = i * 64 / size;
        printf(" ^ ((h%s << %d) | (h%s >> %d))", field_names[i], shift, field_names[i], 64-shift);
    }
    printf("; }\n");

    printf("    bool equal(const %s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(">& v0, const %s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(">& v1) const {\n");
    printf("        return v0 == v1; }\n");

    printf("};\n");
    printf("\n");
    printf("\n");

#if !defined(OLD_STYLE)
    // L-Value wrapper:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");

    printf("struct L%s {\n", class_name);

    for (int i = 0; i < size; i++)
        printf("    %s& %s;\n", type_names[i], field_names[i]);
    printf("\n");

    printf("    L%s(", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s& %s_", (i==0)?"":", ", type_names[i], field_names[i]);
    printf(") : ");
    for (int i = 0; i < size; i++)
        printf("%s%s(%s_)", (i==0)?"":", ", field_names[i], field_names[i]);
    printf(" {}\n");

    printf("    L%s& operator=(const %s<", class_name, class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(">& p) {");
    for (int i = 0; i < size; i++)
        printf(" %s = p.%s;", field_names[i], field_names[i]);
    printf(" return *this; }\n");

    printf("};\n");
    printf("\n");
    printf("\n");

    // Factory function:
    printf("template<");
    for (int i = 0; i < size; i++)
        printf("%sclass %s", (i==0)?"":", ", type_names[i]);
    printf(">\n");
    printf("inline L%s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf("> l_%s(", factory_name);
    for (int i = 0; i < size; i++)
        printf("%s%s& %s", (i==0)?"":", ", type_names[i], field_names[i]);
    printf(") {\n");
    printf("    return L%s<", class_name);
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", type_names[i]);
    printf(">(");
    for (int i = 0; i < size; i++)
        printf("%s%s", (i==0)?"":", ", field_names[i]);
    printf("); }\n");
    printf("\n\n");
#endif

/*
template<class Fst_, class Snd_>
struct LPair {    // <<== new name
    Fst& fst;       // <<== & sign
    Snd& snd;

    Pair(const Fst& fst_, const Snd& snd_) : fst(fst_), snd(snd_) {}
    LPair& operator=(const Pair<X, Y>& p) { x = p.fst; y = p.snd; return *this; }
};

template<class X, class Y>
inline LPair<X,Y> lpair(X& x, Y& y) {
    return LPair<X,Y>(x, y); }
*/
}



int main(int argc, char** argv)
{
    FILE* in = fopen("Tuple_header.txt", "rb"); assert(in);
    for(;;){
        int c = fgetc(in);
        if (feof(in)) break;
        putchar(c);
    }
    fclose(in);

#if defined(OLD_STYLE)
    gen("Pair", "Pair_new", 2);
    gen("Trip", "Trip_new", 3);
    gen("Quad", "Quad_new", 4);
#else
    gen("Pair", "tuple", 2);
    gen("Trip", "tuple", 3);
    gen("Quad", "tuple", 4);
#endif

    in = fopen("Tuple_footer.txt", "rb"); assert(in);
    for(;;){
        int c = fgetc(in);
        if (feof(in)) break;
        putchar(c);
    }
    fclose(in);

    return 0;
}
