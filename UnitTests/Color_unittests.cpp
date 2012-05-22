#include "Color.h"
#include <stdexcept>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE ColorTestCase

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( ColorXYZTest );
    CPPUNIT_TEST( ColorxyYTest );
    CPPUNIT_TEST( ColorRGBTest );
    CPPUNIT_TEST( ColorxyzTest );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
    }

protected:

    void ColorXYZTest()
    {
        // check the constructor and array access
        ColorXYZ test1(1.0, 2.0, 3.0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test1[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 2.0, test1[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 3.0, test1[2], 0.00000000000001 );

        Matrix testMat(1.0, 3, 1);
        testMat[1][0] = 2.0;
        testMat[2][0] = 3.0;

        // check the matrix constructors
        ColorXYZ test2(testMat);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test2[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 2.0, test2[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 3.0, test2[2], 0.00000000000001 );
    }

    void ColorxyYTest()
    {
        ColorxyY test1(1.0, 2.0, 3.0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test1[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 2.0, test1[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 3.0, test1[2], 0.00000000000001 );

        ColorXYZ testEqual(1.0, 1.0, 1.0);
        ColorxyY test2(testEqual);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0 / 3.0, test2[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0 / 3.0, test2[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test2[2], 0.00000000000001 );

        ColorXYZ test3(test2);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test3[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test3[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test3[2], 0.00000000000001 );
    }

    void ColorRGBTest()
    {
        ColorRGB test1(1.0, 2.0, 3.0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, test1[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 2.0, test1[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 3.0, test1[2], 0.00000000000001 );
    }

    void ColorxyzTest()
    {
        Colorxyz test1(0.2, 0.5, 0.3);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.2, test1[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.5, test1[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.3, test1[2], 0.00000000000001 );

        ColorXYZ testEqual(1.0, 1.0, 1.0);
        Colorxyz test2(testEqual);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0 / 3.0, test2[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0 / 3.0, test2[1], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0 / 3.0, test2[2], 0.00000000000001 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
