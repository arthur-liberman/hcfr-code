#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>


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
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, 1.1, 0.05 );
        CPPUNIT_ASSERT( 1 == 0 );
        CPPUNIT_ASSERT( 1 == 1 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
