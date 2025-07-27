#include "text.h"
#include <iostream>

int main()
{
    ck::Text text;
    auto& attr = text.prop();
    attr.set("hello","你好");
    attr.set("hello","再见");
    attr.set("","再见");
    attr.set("3525","");
    attr.set("4363444444444444444444444444444444444444444444444444444444444444444444444444444444444444","");
    attr.set("value",3.14f);

    auto group = text.get();
    group->set("hello world","你好世界");

    text.save("H:/test.ckt",true);

    ck::Text text1;
    text1.open("H:/test.ckt");
    auto& attr1 = text.prop();
    group = text1.get();

    for(auto& it : *group)
    {
        std::cout << it.first << ":" << it.second <<  std::endl;
    }
}
