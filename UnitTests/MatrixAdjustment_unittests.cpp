#include "Color.h"
#include <stdexcept>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE MatrixAdjustTestCase

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( MatrixAdjust );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp()
    {
    }

protected:
    void checkSamexy(const ColorxyY& ref, const ColorxyY& toTest)
    {
        CPPUNIT_ASSERT_DOUBLES_EQUAL( ref[0], toTest[0], 0.00000000000001 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( ref[1], toTest[1], 0.00000000000001 );
    }



    void MatrixAdjust()
    {
        // setup a test of the adjustment matrix
        // test is as given by kjgarrison on AVS thread
        ColorXYZ references[3] = {
                                    ColorXYZ(0.64, 0.33, 0.03),
                                    ColorXYZ(0.30, 0.60, 0.10),
                                    ColorXYZ(0.15, 0.06, 0.79)
                                 };
        ColorxyY whiteRefxyY(0.312712, 0.329008, 0.99);
        ColorXYZ whiteRef(whiteRefxyY);
        ColorXYZ measurements[3] = {
                                    ColorXYZ(0.633, 0.334, 0.033),
                                    ColorXYZ(0.294, 0.606, 0.100),
                                    ColorXYZ(0.150, 0.060, 0.790)
                                   };
        ColorxyY whiteTesyxyY(0.305712, 0.329008, 1.00);
        ColorXYZ whiteTest(whiteTesyxyY);

        Matrix convMatrix = ComputeConversionMatrix(measurements, references, whiteTest, whiteRef, false);

        // check the white point comes through exactly back to the ref
        // and that the other points the xy values are the same
        ColorXYZ testWhite(convMatrix * whiteTest);
        ColorxyY testWhitexyY(testWhite);
        checkSamexy(whiteRefxyY, testWhitexyY);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( whiteRefxyY[2], testWhitexyY[2], 0.00000000000001 );

        ColorxyY testResults[3] = {
                                ColorxyY(ColorXYZ(convMatrix * measurements[0])),
                                ColorxyY(ColorXYZ(convMatrix * measurements[1])),
                                ColorxyY(ColorXYZ(convMatrix * measurements[2]))
                            };

        ColorxyY testExpected[3] = {
                                ColorxyY(references[0]),
                                ColorxyY(references[1]),
                                ColorxyY(references[2])
                            };

        for(int i(0); i < 3; ++i)
        {
            checkSamexy(testExpected[i], testResults[i]);
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
