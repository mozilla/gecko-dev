//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderImage_test.cpp:
// Tests for images
//

#include "angle_gl.h"
#include "gtest/gtest.h"
#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/TranslatorESSL.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

namespace
{

// Checks that the imageStore call with mangled name imageStoreMangledName exists in the AST.
// Further each argument is checked whether it matches the expected properties given the compiled
// shader.
void CheckImageStoreCall(TIntermNode *astRoot,
                         const TString &imageStoreMangledName,
                         TBasicType imageType,
                         int storeLocationNominalSize,
                         TBasicType storeValueType,
                         int storeValueNominalSize)
{
    const TIntermAggregate *imageStoreFunctionCall =
        FindFunctionCallNode(astRoot, imageStoreMangledName);
    ASSERT_NE(nullptr, imageStoreFunctionCall);

    const TIntermSequence *storeArguments = imageStoreFunctionCall->getSequence();
    ASSERT_EQ(3u, storeArguments->size());

    const TIntermTyped *storeArgument1Typed = (*storeArguments)[0]->getAsTyped();
    ASSERT_EQ(imageType, storeArgument1Typed->getBasicType());

    const TIntermTyped *storeArgument2Typed = (*storeArguments)[1]->getAsTyped();
    ASSERT_EQ(EbtInt, storeArgument2Typed->getBasicType());
    ASSERT_EQ(storeLocationNominalSize, storeArgument2Typed->getNominalSize());

    const TIntermTyped *storeArgument3Typed = (*storeArguments)[2]->getAsTyped();
    ASSERT_EQ(storeValueType, storeArgument3Typed->getBasicType());
    ASSERT_EQ(storeValueNominalSize, storeArgument3Typed->getNominalSize());
}

// Checks that the imageLoad call with mangled name imageLoadMangledName exists in the AST.
// Further each argument is checked whether it matches the expected properties given the compiled
// shader.
void CheckImageLoadCall(TIntermNode *astRoot,
                        const TString &imageLoadMangledName,
                        TBasicType imageType,
                        int loadLocationNominalSize)
{
    const TIntermAggregate *imageLoadFunctionCall =
        FindFunctionCallNode(astRoot, imageLoadMangledName);
    ASSERT_NE(nullptr, imageLoadFunctionCall);

    const TIntermSequence *loadArguments = imageLoadFunctionCall->getSequence();
    ASSERT_EQ(2u, loadArguments->size());

    const TIntermTyped *loadArgument1Typed = (*loadArguments)[0]->getAsTyped();
    ASSERT_EQ(imageType, loadArgument1Typed->getBasicType());

    const TIntermTyped *loadArgument2Typed = (*loadArguments)[1]->getAsTyped();
    ASSERT_EQ(EbtInt, loadArgument2Typed->getBasicType());
    ASSERT_EQ(loadLocationNominalSize, loadArgument2Typed->getNominalSize());
}

// Checks whether the image is properly exported as a uniform by the compiler.
void CheckExportedImageUniform(const std::vector<sh::Uniform> &uniforms,
                               size_t uniformIndex,
                               ::GLenum imageTypeGL,
                               const TString &imageName)
{
    ASSERT_EQ(1u, uniforms.size());

    const auto &imageUniform = uniforms[uniformIndex];
    ASSERT_EQ(imageTypeGL, imageUniform.type);
    ASSERT_STREQ(imageUniform.name.c_str(), imageName.c_str());
}

// Checks whether the image is saved in the AST as a node with the correct properties given the
// shader.
void CheckImageDeclaration(TIntermNode *astRoot,
                           const TString &imageName,
                           TBasicType imageType,
                           TLayoutImageInternalFormat internalFormat,
                           bool readonly,
                           bool writeonly,
                           bool coherent,
                           bool restrictQualifier,
                           bool volatileQualifier)
{
    const TIntermSymbol *myImageNode = FindSymbolNode(astRoot, imageName, imageType);
    ASSERT_NE(nullptr, myImageNode);

    const TType &myImageType                = myImageNode->getType();
    TLayoutQualifier myImageLayoutQualifier = myImageType.getLayoutQualifier();
    ASSERT_EQ(internalFormat, myImageLayoutQualifier.imageInternalFormat);
    TMemoryQualifier myImageMemoryQualifier = myImageType.getMemoryQualifier();
    ASSERT_EQ(readonly, myImageMemoryQualifier.readonly);
    ASSERT_EQ(writeonly, myImageMemoryQualifier.writeonly);
    ASSERT_EQ(coherent, myImageMemoryQualifier.coherent);
    ASSERT_EQ(restrictQualifier, myImageMemoryQualifier.restrictQualifier);
    ASSERT_EQ(volatileQualifier, myImageMemoryQualifier.volatileQualifier);
}

}  // namespace

class ShaderImageTest : public testing::Test
{
  public:
    ShaderImageTest() {}

  protected:
    virtual void SetUp()
    {
        ShBuiltInResources resources;
        sh::InitBuiltInResources(&resources);

        mTranslator = new sh::TranslatorESSL(GL_COMPUTE_SHADER, SH_GLES3_1_SPEC);
        ASSERT_TRUE(mTranslator->Init(resources));
    }

    virtual void TearDown() { delete mTranslator; }

    // Return true when compilation succeeds
    bool compile(const std::string &shaderString)
    {
        const char *shaderStrings[] = {shaderString.c_str()};
        mASTRoot                    = mTranslator->compileTreeForTesting(shaderStrings, 1,
                                                      SH_INTERMEDIATE_TREE | SH_VARIABLES);
        TInfoSink &infoSink = mTranslator->getInfoSink();
        mInfoLog            = infoSink.info.c_str();
        return mASTRoot != nullptr;
    }

  protected:
    std::string mTranslatedCode;
    std::string mInfoLog;
    sh::TranslatorESSL *mTranslator;
    TIntermNode *mASTRoot;
};

// Test that an image2D is properly parsed and exported as a uniform.
TEST_F(ShaderImageTest, Image2DDeclaration)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 4) in;\n"
        "layout(rgba32f) uniform highp readonly image2D myImage;\n"
        "void main() {\n"
        "   ivec2 sz = imageSize(myImage);\n"
        "}";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed" << mInfoLog;
    }

    CheckExportedImageUniform(mTranslator->getUniforms(), 0, GL_IMAGE_2D, "myImage");
    CheckImageDeclaration(mASTRoot, "myImage", EbtImage2D, EiifRGBA32F, true, false, false, false,
                          false);
}

// Test that an image3D is properly parsed and exported as a uniform.
TEST_F(ShaderImageTest, Image3DDeclaration)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 4) in;\n"
        "layout(rgba32ui) uniform highp writeonly readonly uimage3D myImage;\n"
        "void main() {\n"
        "   ivec3 sz = imageSize(myImage);\n"
        "}";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed" << mInfoLog;
    }

    CheckExportedImageUniform(mTranslator->getUniforms(), 0, GL_UNSIGNED_INT_IMAGE_3D, "myImage");
    CheckImageDeclaration(mASTRoot, "myImage", EbtUImage3D, EiifRGBA32UI, true, true, false, false,
                          false);
}

// Check that imageLoad calls get correctly parsed.
TEST_F(ShaderImageTest, ImageLoad)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 4) in;\n"
        "layout(rgba32f) uniform highp readonly image2D my2DImageInput;\n"
        "layout(rgba32i) uniform highp readonly iimage3D my3DImageInput;\n"
        "void main() {\n"
        "   vec4 result = imageLoad(my2DImageInput, ivec2(gl_LocalInvocationID.xy));\n"
        "   ivec4 result2 = imageLoad(my3DImageInput, ivec3(gl_LocalInvocationID.xyz));\n"
        "}";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed" << mInfoLog;
    }

    // imageLoad call with image2D passed
    CheckImageLoadCall(mASTRoot, "imageLoad(im21;vi2;", EbtImage2D, 2);

    // imageLoad call with image3D passed
    CheckImageLoadCall(mASTRoot, "imageLoad(iim31;vi3;", EbtIImage3D, 3);
}

// Check that imageStore calls get correctly parsed.
TEST_F(ShaderImageTest, ImageStore)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 4) in;\n"
        "layout(rgba32f) uniform highp writeonly image2D my2DImageOutput;\n"
        "layout(rgba32ui) uniform highp writeonly uimage2DArray my2DImageArrayOutput;\n"
        "void main() {\n"
        "   imageStore(my2DImageOutput, ivec2(gl_LocalInvocationID.xy), vec4(0.0));\n"
        "   imageStore(my2DImageArrayOutput, ivec3(gl_LocalInvocationID.xyz), uvec4(0));\n"
        "}";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed" << mInfoLog;
    }

    // imageStore call with image2D
    CheckImageStoreCall(mASTRoot, "imageStore(im21;vi2;vf4;", EbtImage2D, 2, EbtFloat, 4);

    // imageStore call with image2DArray
    CheckImageStoreCall(mASTRoot, "imageStore(uim2a1;vi3;vu4;", EbtUImage2DArray, 3, EbtUInt, 4);
}

// Check that memory qualifiers are correctly parsed.
TEST_F(ShaderImageTest, ImageMemoryQualifiers)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(local_size_x = 4) in;"
        "layout(rgba32f) uniform highp coherent readonly image2D image1;\n"
        "layout(rgba32f) uniform highp volatile writeonly image2D image2;\n"
        "layout(rgba32f) uniform highp volatile restrict readonly writeonly image2D image3;\n"
        "void main() {\n"
        "}";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed" << mInfoLog;
    }

    CheckImageDeclaration(mASTRoot, "image1", EbtImage2D, EiifRGBA32F, true, false, true, false,
                          false);
    CheckImageDeclaration(mASTRoot, "image2", EbtImage2D, EiifRGBA32F, false, true, true, false,
                          true);
    CheckImageDeclaration(mASTRoot, "image3", EbtImage2D, EiifRGBA32F, true, true, true, true,
                          true);
}
