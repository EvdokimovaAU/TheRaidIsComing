#pragma once

#include "CoreMinimal.h"

namespace RICPlacement
{
    using V = FVector2D;

    constexpr double Epsilon = 0.001;

    inline double Dot(const V& A, const V& B)
    {
        return A.X * B.X + A.Y * B.Y;
    }

    inline double Cross(const V& A, const V& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    inline V Perp(const V& A)
    {
        return V(-A.Y, A.X);
    }

    struct FRegion
    {
        bool bRing = false;

        V ZoneHalf = V(400.0, 300.0);
        V BuildingHalf = V(50.0, 50.0);

        V AxisX = V(1.0, 0.0);
        V AxisY = V(0.0, 1.0);

        double Inner = 250.0;
        double Outer = 600.0;
        double StartDegrees = 0.0;
        double ArcDegrees = 120.0;
    };

    struct FBoundary
    {
        bool bCircle;
        V Vector;
        double Value;
    };

    inline bool FindNearest(
        const FRegion& R,
        const V& Wanted,
        V& Out)
    {
        const V X = R.AxisX * R.BuildingHalf.X;
        const V Y = R.AxisY * R.BuildingHalf.Y;

        const V Corners[] =
        {
            X + Y,
            X - Y,
            -X + Y,
            -X - Y
        };

        // Прямоугольная зона: уменьшаем доступное пространство
        // на размеры основания постройки.
        if (!R.bRing)
        {
            const V Extent(
                FMath::Abs(X.X) + FMath::Abs(Y.X),
                FMath::Abs(X.Y) + FMath::Abs(Y.Y));

            const V Limit = R.ZoneHalf - Extent;

            if (Limit.X < 0.0 || Limit.Y < 0.0)
            {
                return false;
            }

            Out = V(
                FMath::Clamp(Wanted.X, -Limit.X, Limit.X),
                FMath::Clamp(Wanted.Y, -Limit.Y, Limit.Y));

            return true;
        }

        auto IsValid = [&](const V& P)
            {
                // Ближайшая точка прямоугольника к центру кольца.
                // Проверки одних углов для внутренней дуги недостаточно.
                const double DX = FMath::Max(
                    FMath::Abs(Dot(P, R.AxisX)) - R.BuildingHalf.X,
                    0.0);

                const double DY = FMath::Max(
                    FMath::Abs(Dot(P, R.AxisY)) - R.BuildingHalf.Y,
                    0.0);

                if (FMath::Sqrt(DX * DX + DY * DY)
                    < R.Inner - Epsilon)
                {
                    return false;
                }

                // Все углы должны находиться внутри внешнего радиуса.
                for (const V& C : Corners)
                {
                    if ((P + C).SizeSquared()
                    > FMath::Square(R.Outer + Epsilon))
                    {
                        return false;
                    }
                }

                if (R.ArcDegrees >= 360.0)
                {
                    return true;
                }

                // Проверяем весь угловой диапазон основания.
                // Работает и для секторов шире 180 градусов.
                double CenterAngle =
                    FMath::RadiansToDegrees(FMath::Atan2(P.Y, P.X))
                    - R.StartDegrees;

                CenterAngle = FMath::Fmod(CenterAngle, 360.0);

                if (CenterAngle < 0.0)
                {
                    CenterAngle += 360.0;
                }

                double Low = 1.e30;
                double High = -1.e30;

                for (const V& C : Corners)
                {
                    const V Q = P + C;

                    const double Angle = CenterAngle
                        + FMath::RadiansToDegrees(
                            FMath::Atan2(Cross(P, Q), Dot(P, Q)));

                    Low = FMath::Min(Low, Angle);
                    High = FMath::Max(High, Angle);
                }

                for (int32 Shift = -1; Shift <= 1; ++Shift)
                {
                    if (Low + 360.0 * Shift >= -1.e-7
                        && High + 360.0 * Shift
                        <= R.ArcDegrees + 1.e-7)
                    {
                        return true;
                    }
                }

                return false;
            };

        if (IsValid(Wanted))
        {
            Out = Wanted;
            return true;
        }

        // Допустимая область ограничена отрезками и дугами.
        // Проверяем проекции на их границы и точки пересечения.
        TArray<FBoundary> Boundaries;

        auto Circle = [&](const V& Center, double Radius)
            {
                Boundaries.Add(FBoundary{ true, Center, Radius });
            };

        auto Line = [&](const V& Normal, double Distance)
            {
                Boundaries.Add(FBoundary{ false, Normal, Distance });
            };

        for (const V& C : Corners)
        {
            Circle(-C, R.Outer);
            Circle(-C, R.Inner);
        }

        for (double Sign : {-1.0, 1.0})
        {
            Line(
                R.AxisX,
                Sign * (R.BuildingHalf.X + R.Inner));

            Line(
                R.AxisY,
                Sign * (R.BuildingHalf.Y + R.Inner));
        }

        if (R.ArcDegrees < 360.0)
        {
            for (double Degrees :
            {R.StartDegrees, R.StartDegrees + R.ArcDegrees})
            {
                const double Angle =
                    FMath::DegreesToRadians(Degrees);

                const V Normal(
                    -FMath::Sin(Angle),
                    FMath::Cos(Angle));

                for (const V& C : Corners)
                {
                    Line(Normal, -Dot(Normal, C));
                }
            }
        }

        bool bFound = false;
        double BestDistance = 1.e30;

        auto Consider = [&](const V& P)
            {
                const double Distance = (P - Wanted).SizeSquared();

                if (Distance < BestDistance && IsValid(P))
                {
                    BestDistance = Distance;
                    Out = P;
                    bFound = true;
                }
            };

        for (const FBoundary& B : Boundaries)
        {
            if (B.bCircle)
            {
                const V Delta = Wanted - B.Vector;
                const double Length = Delta.Size();

                const V Direction = Length > 1.e-9
                    ? Delta / Length
                    : V(1.0, 0.0);

                Consider(B.Vector + Direction * B.Value);
            }
            else
            {
                Consider(
                    Wanted + B.Vector
                    * (B.Value - Dot(B.Vector, Wanted)));
            }
        }

        for (int32 I = 0; I < Boundaries.Num(); ++I)
        {
            for (int32 J = I + 1; J < Boundaries.Num(); ++J)
            {
                const FBoundary& A = Boundaries[I];
                const FBoundary& B = Boundaries[J];

                if (!A.bCircle && !B.bCircle)
                {
                    const double D = Cross(A.Vector, B.Vector);

                    if (FMath::Abs(D) > 1.e-10)
                    {
                        Consider(V(
                            A.Value * B.Vector.Y
                            - A.Vector.Y * B.Value,
                            A.Vector.X * B.Value
                            - A.Value * B.Vector.X) / D);
                    }
                }
                else if (A.bCircle && B.bCircle)
                {
                    const V Delta = B.Vector - A.Vector;
                    const double D = Delta.Size();

                    if (D < 1.e-9)
                    {
                        continue;
                    }

                    const double Along =
                        (A.Value * A.Value
                            - B.Value * B.Value + D * D)
                        / (2.0 * D);

                    const double HeightSquared =
                        A.Value * A.Value - Along * Along;

                    if (HeightSquared < -1.e-6)
                    {
                        continue;
                    }

                    const V Base =
                        A.Vector + Delta * (Along / D);

                    const V Side = Perp(Delta / D)
                        * FMath::Sqrt(
                            FMath::Max(HeightSquared, 0.0));

                    Consider(Base + Side);
                    Consider(Base - Side);
                }
                else
                {
                    const FBoundary& C = A.bCircle ? A : B;
                    const FBoundary& L = A.bCircle ? B : A;

                    const double D =
                        L.Value - Dot(L.Vector, C.Vector);

                    const double HeightSquared =
                        C.Value * C.Value - D * D;

                    if (HeightSquared < -1.e-6)
                    {
                        continue;
                    }

                    const V Base = C.Vector + L.Vector * D;

                    const V Side = Perp(L.Vector)
                        * FMath::Sqrt(
                            FMath::Max(HeightSquared, 0.0));

                    Consider(Base + Side);
                    Consider(Base - Side);
                }
            }
        }

        return bFound;
    }
}