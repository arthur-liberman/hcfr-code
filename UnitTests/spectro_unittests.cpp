#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>
#include <numsup.h>

#define THIS_TEST_CASE SpectroTestCase

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( floatConversion );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
    }

protected:
    void floatConversion()
    {
        // test that the conversion from int to double
        // works correctly, was having issues with this function inside
        // the spectro lib
        double result = IEEE754todouble(1034279276);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.080981105566024780, result, 0.00000000000001 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
