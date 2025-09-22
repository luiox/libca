#include "Format.hpp"
#include "../test/Test.hpp"

TEST_CASE("Format")
{
    // basics
    ASSERT_EQUAL(fmt11("Hello"), "Hello");
    ASSERT_EQUAL(fmt11("Hello {} {}", 123, "456"), "Hello 123 456");   // equivalent to {0} {1}
    ASSERT_EQUAL(fmt11("Hello {0} {1}", 123, "456"), "Hello 123 456");
    ASSERT_EQUAL(fmt11("Hello {1} {0}", 123, "456"), "Hello 456 123");

    // indices and reordering
    ASSERT_EQUAL(fmt11("Hello {1} {0}", 123, "world"), "Hello world 123");
    ASSERT_EQUAL(fmt11("Hello {0} {1}", 123, "world"), "Hello 123 world");
    ASSERT_EQUAL(fmt11("{0}{1}{0}", "abra", "cad"), "abracadabra");

    // basic formatting: dec, hex, oct, prefixes, uppercase
    ASSERT_EQUAL(fmt11("Hello {:d} {:x} {:o}", 42, 42, 42), "Hello 42 2a 52");
    ASSERT_EQUAL(fmt11("Hello {:#d} {:#x} {:#o}", 42, 42, 42), "Hello 42 0x2a 052");
    ASSERT_EQUAL(fmt11("Hello {0:d} {0:x} {0:o}", 42, 42, 42), "Hello 42 2a 52");
    ASSERT_EQUAL(fmt11("Hello {0:d} {0:#x} {0:#o}", 42, 42, 42), "Hello 42 0x2a 052");
    ASSERT_EQUAL(fmt11("Hello {:D} {:X} {:O}", 42, 42, 42), "Hello 42 2A 52");
    ASSERT_EQUAL(fmt11("Hello {:#D} {:#X} {:#O}", 42, 42, 42), "Hello 42 0X2A 052");
    ASSERT_EQUAL(fmt11("Hello {0:D} {0:X} {0:O}", 42, 42, 42), "Hello 42 2A 52");
    ASSERT_EQUAL(fmt11("Hello {0:D} {0:#X} {0:#O}", 42, 42, 42), "Hello 42 0X2A 052");
    ASSERT_EQUAL(fmt11("Hello {:d} {:x} {:o} {:#x} {:#o} {:#X} {:#O}", 42, 42, 42, 42, 42, 42, 42),
                 "Hello 42 2a 52 0x2a 052 0X2A 052");

    // extra formatting: width, decimals, custom prefixes and alignment
    ASSERT_EQUAL(fmt11("Hello {:8} {:12} {:16}", 3.14159, 3.14159, 3.14159),
                 "Hello        3            3                3");
    ASSERT_EQUAL(fmt11("Hello {:8.2} {:12.3} {:16.4}", 3.14159, 3.14159, 3.14159),
                 "Hello     3.14        3.142           3.1416");
    ASSERT_EQUAL(fmt11("Hello {:*8.2} {:|12.3} {:.16.4}", 3.14159, 3.14159, 3.14159),
                 "Hello ****3.14 |||||||3.142 ..........3.1416");
    ASSERT_EQUAL(fmt11("Hello {:<*8.2} {:<|12.3} {:<.16.4}", 3.14159, 3.14159, 3.14159),
                 "Hello 3.14**** 3.142||||||| 3.1416..........");
    ASSERT_EQUAL(fmt11("Hello {:.>10} Emmett {:!<10}", "Doc", "Brown"),
                 "Hello .......Doc Emmett Brown!!!!!");

    // context and {{mustaches}}
    std::map<std::string, std::string> ctx{{"player1", "John"}, {"player2", "Doe"}};
    ASSERT_EQUAL(fmt11map(ctx, "Hello {{player1}} & {{player2}}!!"), "Hello John & Doe!!");
    ASSERT_EQUAL(fmt11map(ctx, "Hello {{player1}} & {{player2}}!! {0} {} {1}!!", 123, 456),
                 "Hello John & Doe!! 123 123 456!!");
    ASSERT_EQUAL(fmt11map(ctx, "Hello {{player1}:*>10} & {{player2}:*<10}!!"),
                 "Hello ******John & Doe*******!!");
    ASSERT_EQUAL(fmt11map(ctx, "{{player1}}{{player2}}"), "JohnDoe");
    ctx.clear();

    // check for malformed input
    ASSERT_EQUAL(fmt11(0), "");                     // null ptr
    ASSERT_EQUAL(fmt11(""), "");                    // empty string
    ASSERT_EQUAL(fmt11("{}{}{}"), "{}{}{}");        // no args
    ASSERT_EQUAL(fmt11("{}{}{}", 1, 2), "12{}");    // not enough args
    ASSERT_EQUAL(fmt11("", 1, 2, 3), "");           // too many args
    ASSERT_EQUAL(fmt11("{}", 1, 2, 3), "1");        // too many args
    ASSERT_EQUAL(fmt11("{"), "{");                  // unbalanced
    ASSERT_EQUAL(fmt11("}"), "}");                  // unbalanced
    ASSERT_EQUAL(fmt11("{{"), "{{");                // unbalanced
    ASSERT_EQUAL(fmt11("}}"), "}}");                // unbalanced
    ASSERT_EQUAL(fmt11("{{{"), "{{{");              // unbalanced
    ASSERT_EQUAL(fmt11("}}}"), "}}}");              // unbalanced
    ASSERT_EQUAL(fmt11("}{"), "}{");                // mismatch
    ASSERT_EQUAL(fmt11("}{}"), "}{}");              // mismatch
    ASSERT_EQUAL(fmt11("}}{"), "}}{");              // mismatch
    ASSERT_EQUAL(fmt11("{}{"), "{}{");              // mismatch
    ASSERT_EQUAL(fmt11("{}}"), "{}}");              // mismatch
    ASSERT_EQUAL(fmt11("{\t}", "hello"), "{\t}");   // invalid identifier
    ASSERT_EQUAL(fmt11("{\"{0}\"}", "hello"),
                 "{\"hello\"}");   // false positive \" is not part of an identifier
    ASSERT_EQUAL(fmt11(std::string(128, '{').c_str()), std::string(128, '{'));   // buffer overflow
    ASSERT_EQUAL(fmt11(std::string(128, '}').c_str()), std::string(128, '}'));   // buffer overflow
    ASSERT_EQUAL(fmt11("{{player1}}{{player2}}"), "{{player1}}{{player2}}");     // missing map
    ASSERT_EQUAL(fmt11map(ctx, "{{player1}}{{player2}}"), "{{player1}}{{player2}}");   // empty map
}