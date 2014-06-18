/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MATRIX_H_
#define MOZILLA_GFX_MATRIX_H_

#include "Types.h"
#include "Rect.h"
#include "Point.h"
#include <math.h>

namespace mozilla {
namespace gfx {

class Matrix
{
public:
  Matrix()
    : _11(1.0f), _12(0)
    , _21(0), _22(1.0f)
    , _31(0), _32(0)
  {}
  Matrix(Float a11, Float a12, Float a21, Float a22, Float a31, Float a32)
    : _11(a11), _12(a12)
    , _21(a21), _22(a22)
    , _31(a31), _32(a32)
  {}
  Float _11, _12;
  Float _21, _22;
  Float _31, _32;

  Point operator *(const Point &aPoint) const
  {
    Point retPoint;

    retPoint.x = aPoint.x * _11 + aPoint.y * _21 + _31;
    retPoint.y = aPoint.x * _12 + aPoint.y * _22 + _32;

    return retPoint;
  }

  Size operator *(const Size &aSize) const
  {
    Size retSize;

    retSize.width = aSize.width * _11 + aSize.height * _21;
    retSize.height = aSize.width * _12 + aSize.height * _22;

    return retSize;
  }

  GFX2D_API Rect TransformBounds(const Rect& rect) const;

  // Apply a scale to this matrix. This scale will be applied -before- the
  // existing transformation of the matrix.
  Matrix &Scale(Float aX, Float aY)
  {
    _11 *= aX;
    _12 *= aX;
    _21 *= aY;
    _22 *= aY;

    return *this;
  }

  Matrix &Translate(Float aX, Float aY)
  {
    _31 += _11 * aX + _21 * aY;
    _32 += _12 * aX + _22 * aY;

    return *this;
  }

  Matrix &PostTranslate(Float aX, Float aY)
  {
    _31 += aX;
    _32 += aY;
    return *this;
  }

  Matrix &Rotate(Float aAngle)
  {
    return *this = Matrix::Rotation(aAngle) * *this;
  }

  bool Invert()
  {
    // Compute co-factors.
    Float A = _22;
    Float B = -_21;
    Float C = _21 * _32 - _22 * _31;
    Float D = -_12;
    Float E = _11;
    Float F = _31 * _12 - _11 * _32;

    Float det = Determinant();

    if (!det) {
      return false;
    }

    Float inv_det = 1 / det;

    _11 = inv_det * A;
    _12 = inv_det * D;
    _21 = inv_det * B;
    _22 = inv_det * E;
    _31 = inv_det * C;
    _32 = inv_det * F;

    return true;
  }

  Float Determinant() const
  {
    return _11 * _22 - _12 * _21;
  }

  static Matrix Translation(Float aX, Float aY)
  {
    return Matrix(1.0f, 0.0f, 0.0f, 1.0f, aX, aY);
  }

  static Matrix Translation(Point aPoint)
  {
    return Translation(aPoint.x, aPoint.y);
  }

  GFX2D_API static Matrix Rotation(Float aAngle);

  static Matrix Scaling(Float aX, Float aY)
  {
    return Matrix(aX, 0.0f, 0.0f, aY, 0.0f, 0.0f);
  }

  Matrix operator*(const Matrix &aMatrix) const
  {
    Matrix resultMatrix;

    resultMatrix._11 = this->_11 * aMatrix._11 + this->_12 * aMatrix._21;
    resultMatrix._12 = this->_11 * aMatrix._12 + this->_12 * aMatrix._22;
    resultMatrix._21 = this->_21 * aMatrix._11 + this->_22 * aMatrix._21;
    resultMatrix._22 = this->_21 * aMatrix._12 + this->_22 * aMatrix._22;
    resultMatrix._31 = this->_31 * aMatrix._11 + this->_32 * aMatrix._21 + aMatrix._31;
    resultMatrix._32 = this->_31 * aMatrix._12 + this->_32 * aMatrix._22 + aMatrix._32;

    return resultMatrix;
  }

  Matrix& operator*=(const Matrix &aMatrix)
  {
    Matrix resultMatrix = *this * aMatrix;
    return *this = resultMatrix;
  }

  /* Returns true if the other matrix is fuzzy-equal to this matrix.
   * Note that this isn't a cheap comparison!
   */
  bool operator==(const Matrix& other) const
  {
    return FuzzyEqual(_11, other._11) && FuzzyEqual(_12, other._12) &&
           FuzzyEqual(_21, other._21) && FuzzyEqual(_22, other._22) &&
           FuzzyEqual(_31, other._31) && FuzzyEqual(_32, other._32);
  }

  bool operator!=(const Matrix& other) const
  {
    return !(*this == other);
  }

  /* Returns true if the matrix is a rectilinear transformation (i.e.
   * grid-aligned rectangles are transformed to grid-aligned rectangles)
   */
  bool IsRectilinear() const {
    if (FuzzyEqual(_12, 0) && FuzzyEqual(_21, 0)) {
      return true;
    } else if (FuzzyEqual(_22, 0) && FuzzyEqual(_11, 0)) {
      return true;
    }

    return false;
  }

  /**
   * Returns true if the matrix is anything other than a straight
   * translation by integers.
  */
  bool HasNonIntegerTranslation() const {
    return HasNonTranslation() ||
      !FuzzyEqual(_31, floor(_31 + 0.5)) ||
      !FuzzyEqual(_32, floor(_32 + 0.5));
  }

  /**
   * Returns true if the matrix has any transform other
   * than a straight translation.
   */
  bool HasNonTranslation() const {
    return !FuzzyEqual(_11, 1.0) || !FuzzyEqual(_22, 1.0) ||
           !FuzzyEqual(_12, 0.0) || !FuzzyEqual(_21, 0.0);
  }

  /* Returns true if the matrix is an identity matrix.
   */
  bool IsIdentity() const
  {
    return _11 == 1.0f && _12 == 0.0f &&
           _21 == 0.0f && _22 == 1.0f &&
           _31 == 0.0f && _32 == 0.0f;
  }

  /* Returns true if the matrix is singular.
   */
  bool IsSingular() const
  {
    return Determinant() == 0;
  }

  GFX2D_API void NudgeToIntegers();

  bool IsTranslation() const
  {
    return FuzzyEqual(_11, 1.0f) && FuzzyEqual(_12, 0.0f) &&
           FuzzyEqual(_21, 0.0f) && FuzzyEqual(_22, 1.0f);
  }

  bool IsIntegerTranslation() const
  {
    return IsTranslation() &&
           FuzzyEqual(_31, floorf(_31 + 0.5f)) &&
           FuzzyEqual(_32, floorf(_32 + 0.5f));
  }

  Point GetTranslation() const {
    return Point(_31, _32);
  }

  /**
   * Returns true if matrix is multiple of 90 degrees rotation with flipping,
   * scaling and translation.
   */
  bool PreservesAxisAlignedRectangles() const {
      return ((FuzzyEqual(_11, 0.0) && FuzzyEqual(_22, 0.0))
          || (FuzzyEqual(_12, 0.0) && FuzzyEqual(_21, 0.0)));
  }

  /**
   * Returns true if the matrix has any transform other
   * than a translation or scale; this is, if there is
   * no rotation.
   */
  bool HasNonAxisAlignedTransform() const {
      return !FuzzyEqual(_21, 0.0) || !FuzzyEqual(_12, 0.0);
  }

  /**
   * Returns true if the matrix has non-integer scale
   */
  bool HasNonIntegerScale() const {
      return !FuzzyEqual(_11, floor(_11 + 0.5)) ||
             !FuzzyEqual(_22, floor(_22 + 0.5));
  }

private:
  static bool FuzzyEqual(Float aV1, Float aV2) {
    // XXX - Check if fabs does the smart thing and just negates the sign bit.
    return fabs(aV2 - aV1) < 1e-6;
  }
};

class Matrix4x4
{
public:
  Matrix4x4()
    : _11(1.0f), _12(0.0f), _13(0.0f), _14(0.0f)
    , _21(0.0f), _22(1.0f), _23(0.0f), _24(0.0f)
    , _31(0.0f), _32(0.0f), _33(1.0f), _34(0.0f)
    , _41(0.0f), _42(0.0f), _43(0.0f), _44(1.0f)
  {}

  Float _11, _12, _13, _14;
  Float _21, _22, _23, _24;
  Float _31, _32, _33, _34;
  Float _41, _42, _43, _44;

  /**
   * Returns true if the matrix is isomorphic to a 2D affine transformation.
   */
  bool Is2D() const
  {
    if (_13 != 0.0f || _14 != 0.0f ||
        _23 != 0.0f || _24 != 0.0f ||
        _31 != 0.0f || _32 != 0.0f || _33 != 1.0f || _34 != 0.0f ||
        _43 != 0.0f || _44 != 1.0f) {
      return false;
    }
    return true;
  }

  bool Is2D(Matrix* aMatrix) const {
    if (!Is2D()) {
      return false;
    }
    if (aMatrix) {
      aMatrix->_11 = _11;
      aMatrix->_12 = _12;
      aMatrix->_21 = _21;
      aMatrix->_22 = _22;
      aMatrix->_31 = _41;
      aMatrix->_32 = _42;
    }
    return true;
  }

  Matrix As2D() const
  {
    MOZ_ASSERT(Is2D(), "Matrix is not a 2D affine transform");

    return Matrix(_11, _12, _21, _22, _41, _42);
  }

  bool CanDraw2D(Matrix* aMatrix = nullptr) const {
    if (_14 != 0.0f ||
        _24 != 0.0f ||
        _44 != 1.0f) {
      return false;
    }
    if (aMatrix) {
      aMatrix->_11 = _11;
      aMatrix->_12 = _12;
      aMatrix->_21 = _21;
      aMatrix->_22 = _22;
      aMatrix->_31 = _41;
      aMatrix->_32 = _42;
    }
    return true;
  }

  Matrix4x4& ProjectTo2D() {
    _31 = 0.0f;
    _32 = 0.0f;
    _13 = 0.0f;
    _23 = 0.0f;
    _33 = 1.0f;
    _43 = 0.0f;
    _34 = 0.0f;
    return *this;
  }

  static Matrix4x4 From2D(const Matrix &aMatrix) {
    Matrix4x4 matrix;
    matrix._11 = aMatrix._11;
    matrix._12 = aMatrix._12;
    matrix._21 = aMatrix._21;
    matrix._22 = aMatrix._22;
    matrix._41 = aMatrix._31;
    matrix._42 = aMatrix._32;
    return matrix;
  }

  bool Is2DIntegerTranslation() const
  {
    return Is2D() && As2D().IsIntegerTranslation();
  }

  Point4D operator *(const Point4D& aPoint) const
  {
    Point4D retPoint;

    retPoint.x = aPoint.x * _11 + aPoint.y * _21 + aPoint.z * _31 + _41;
    retPoint.y = aPoint.x * _12 + aPoint.y * _22 + aPoint.z * _32 + _42;
    retPoint.z = aPoint.x * _13 + aPoint.y * _23 + aPoint.z * _33 + _43;
    retPoint.w = aPoint.x * _14 + aPoint.y * _24 + aPoint.z * _34 + _44;

    return retPoint;
  }

  Point3D operator *(const Point3D& aPoint) const
  {
    Point4D temp(aPoint.x, aPoint.y, aPoint.z, 1);

    temp = *this * temp;
    temp /= temp.w;

    return Point3D(temp.x, temp.y, temp.z);
  }

  Point operator *(const Point &aPoint) const
  {
    Point4D temp(aPoint.x, aPoint.y, 0, 1);

    temp = *this * temp;
    temp /= temp.w;

    return Point(temp.x, temp.y);
  }

  GFX2D_API Rect TransformBounds(const Rect& rect) const;

  // Apply a scale to this matrix. This scale will be applied -before- the
  // existing transformation of the matrix.
  Matrix4x4 &Scale(Float aX, Float aY, Float aZ)
  {
    _11 *= aX;
    _12 *= aX;
    _13 *= aX;
    _21 *= aY;
    _22 *= aY;
    _23 *= aY;
    _31 *= aZ;
    _32 *= aZ;
    _33 *= aZ;

    return *this;
  }

  Matrix4x4 &Translate(Float aX, Float aY, Float aZ)
  {
    _41 += aX * _11 + aY * _21 + aZ * _31;
    _42 += aX * _12 + aY * _22 + aZ * _32;
    _43 += aX * _13 + aY * _23 + aZ * _33;
    _44 += aX * _14 + aY * _24 + aZ * _34;

    return *this;
  }

  bool operator==(const Matrix4x4& o) const
  {
    // XXX would be nice to memcmp here, but that breaks IEEE 754 semantics
    return _11 == o._11 && _12 == o._12 && _13 == o._13 && _14 == o._14 &&
           _21 == o._21 && _22 == o._22 && _23 == o._23 && _24 == o._24 &&
           _31 == o._31 && _32 == o._32 && _33 == o._33 && _34 == o._34 &&
           _41 == o._41 && _42 == o._42 && _43 == o._43 && _44 == o._44;
  }

  bool operator!=(const Matrix4x4& o) const
  {
    return !((*this) == o);
  }

  Matrix4x4 operator*(const Matrix4x4 &aMatrix) const
  {
    Matrix4x4 matrix;

    matrix._11 = _11 * aMatrix._11 + _12 * aMatrix._21 + _13 * aMatrix._31 + _14 * aMatrix._41;
    matrix._21 = _21 * aMatrix._11 + _22 * aMatrix._21 + _23 * aMatrix._31 + _24 * aMatrix._41;
    matrix._31 = _31 * aMatrix._11 + _32 * aMatrix._21 + _33 * aMatrix._31 + _34 * aMatrix._41;
    matrix._41 = _41 * aMatrix._11 + _42 * aMatrix._21 + _43 * aMatrix._31 + _44 * aMatrix._41;
    matrix._12 = _11 * aMatrix._12 + _12 * aMatrix._22 + _13 * aMatrix._32 + _14 * aMatrix._42;
    matrix._22 = _21 * aMatrix._12 + _22 * aMatrix._22 + _23 * aMatrix._32 + _24 * aMatrix._42;
    matrix._32 = _31 * aMatrix._12 + _32 * aMatrix._22 + _33 * aMatrix._32 + _34 * aMatrix._42;
    matrix._42 = _41 * aMatrix._12 + _42 * aMatrix._22 + _43 * aMatrix._32 + _44 * aMatrix._42;
    matrix._13 = _11 * aMatrix._13 + _12 * aMatrix._23 + _13 * aMatrix._33 + _14 * aMatrix._43;
    matrix._23 = _21 * aMatrix._13 + _22 * aMatrix._23 + _23 * aMatrix._33 + _24 * aMatrix._43;
    matrix._33 = _31 * aMatrix._13 + _32 * aMatrix._23 + _33 * aMatrix._33 + _34 * aMatrix._43;
    matrix._43 = _41 * aMatrix._13 + _42 * aMatrix._23 + _43 * aMatrix._33 + _44 * aMatrix._43;
    matrix._14 = _11 * aMatrix._14 + _12 * aMatrix._24 + _13 * aMatrix._34 + _14 * aMatrix._44;
    matrix._24 = _21 * aMatrix._14 + _22 * aMatrix._24 + _23 * aMatrix._34 + _24 * aMatrix._44;
    matrix._34 = _31 * aMatrix._14 + _32 * aMatrix._24 + _33 * aMatrix._34 + _34 * aMatrix._44;
    matrix._44 = _41 * aMatrix._14 + _42 * aMatrix._24 + _43 * aMatrix._34 + _44 * aMatrix._44;

    return matrix;
  }


  /* Returns true if the matrix is an identity matrix.
   */
  bool IsIdentity() const
  {
    return _11 == 1.0f && _12 == 0.0f && _13 == 0.0f && _14 == 0.0f &&
           _21 == 0.0f && _22 == 1.0f && _23 == 0.0f && _24 == 0.0f &&
           _31 == 0.0f && _32 == 0.0f && _33 == 1.0f && _34 == 0.0f &&
           _41 == 0.0f && _42 == 0.0f && _43 == 0.0f && _44 == 1.0f;
  }

  bool IsSingular() const
  {
    return Determinant() == 0.0;
  }

  Float Determinant() const
  {
    return _14 * _23 * _32 * _41
         - _13 * _24 * _32 * _41
         - _14 * _22 * _33 * _41
         + _12 * _24 * _33 * _41
         + _13 * _22 * _34 * _41
         - _12 * _23 * _34 * _41
         - _14 * _23 * _31 * _42
         + _13 * _24 * _31 * _42
         + _14 * _21 * _33 * _42
         - _11 * _24 * _33 * _42
         - _13 * _21 * _34 * _42
         + _11 * _23 * _34 * _42
         + _14 * _22 * _31 * _43
         - _12 * _24 * _31 * _43
         - _14 * _21 * _32 * _43
         + _11 * _24 * _32 * _43
         + _12 * _21 * _34 * _43
         - _11 * _22 * _34 * _43
         - _13 * _22 * _31 * _44
         + _12 * _23 * _31 * _44
         + _13 * _21 * _32 * _44
         - _11 * _23 * _32 * _44
         - _12 * _21 * _33 * _44
         + _11 * _22 * _33 * _44;
  }

};

class Matrix5x4
{
public:
  Matrix5x4()
    : _11(1.0f), _12(0), _13(0), _14(0)
    , _21(0), _22(1.0f), _23(0), _24(0)
    , _31(0), _32(0), _33(1.0f), _34(0)
    , _41(0), _42(0), _43(0), _44(1.0f)
    , _51(0), _52(0), _53(0), _54(0)
  {}
  Matrix5x4(Float a11, Float a12, Float a13, Float a14,
         Float a21, Float a22, Float a23, Float a24,
         Float a31, Float a32, Float a33, Float a34,
         Float a41, Float a42, Float a43, Float a44,
         Float a51, Float a52, Float a53, Float a54)
    : _11(a11), _12(a12), _13(a13), _14(a14)
    , _21(a21), _22(a22), _23(a23), _24(a24)
    , _31(a31), _32(a32), _33(a33), _34(a34)
    , _41(a41), _42(a42), _43(a43), _44(a44)
    , _51(a51), _52(a52), _53(a53), _54(a54)
  {}
  Float _11, _12, _13, _14;
  Float _21, _22, _23, _24;
  Float _31, _32, _33, _34;
  Float _41, _42, _43, _44;
  Float _51, _52, _53, _54;
};

}
}

#endif /* MOZILLA_GFX_MATRIX_H_ */
