
/****** Object:  UserDefinedFunction [dbo].[f_EasterSunday]    Script Date: 18/02/2026 15:12:05 ******/
SET ANSI_NULLS ON
GO

SET QUOTED_IDENTIFIER ON
GO


/*
    Returns the date of Easter Sunday for the given year number.
    Easter Sunday is the first Sunday after the full moon date that falls on or after 21st March.
 */
CREATE FUNCTION [dbo].[f_EasterSunday] (@YearNo INT)
RETURNS DATE
AS
BEGIN
    declare @a int = @YearNo % 19
    declare @b int = @YearNo / 100
    declare @c int = @YearNo % 100
    declare @d int = @b / 4
    declare @e int = @b % 4
    declare @f int = (@b + 8) / 25
    declare @g int = (@b - @f + 1) / 3
    declare @h int = (19*@a + @b - @d - @g + 15) % 30
    declare @i int = @c / 4
    declare @k int = @c % 4
    declare @l int = (32 + 2*@e + 2*@i - @h - @k) % 7
    declare @m int = (@a + 11*@h + 22*@l) / 451
    declare @n int = @h + @l - 7*@m + 114
    declare @month int = @n / 31
    declare @day int = (@n % 31) + 1

    return datefromparts(@YearNo, @month, @day)
END
GO

CREATE FUNCTION [dbo].[ft_SunriseAndSetTimes](@Date DATE)
RETURNS @SUNRISEANDSETTIMES TABLE (
    Sunrise TIME,
    Sunset  TIME
)
AS
BEGIN
    if @Date is null return

    declare @hours_past_local_midnight float = 0.5
    declare @latitude float = 52.7073
    declare @longitude float = -2.7553

    declare @month int = datepart(month, @date)
    declare @sunday int = datepart(weekday, convert(date, '2019-12-22', 126))
    declare @previous_sunday int = datepart(day, @date) - (datepart(weekday, @date) % 7)
    declare @daylight_savings float = case
                when @month < 3 or @month > 10 then 0.0
                when @month > 3 and @month < 10 then 1.0
                when @month = 3 then iif(@previous_sunday > 24, 1.0, 0.0)
                else iif(@previous_sunday > 24, 0.0, 1.0)
            end

    declare @timezone float = @daylight_savings
    declare @julian_day float = convert(float, convert(datetime, @date)) + 2415021.0
    declare @julian_century float = (@julian_day - 2451545.0)/36525.0
    declare @geom_mean_long_sun float = convert(decimal(38,19), 280.46646 + @julian_century*(36000.76983 + @julian_century*0.0003032)) % 360
    declare @geom_mean_anom_sun float = 357.52911 + @julian_century*(35999.05029 - 0.0001537*@julian_century)
    declare @eccent_earth_orbit float = 0.016708634 - @julian_century*(0.000042037 + 0.0000001267*@julian_century)
    declare @sun_eq_of_ctr float = sin(radians(@geom_mean_anom_sun))*(1.914602 - @julian_century*(0.004817 + 0.000014*@julian_century)) + sin(radians(2*@geom_mean_anom_sun))*(0.019993-0.000101*@julian_century) + sin(radians(3*@geom_mean_anom_sun))*0.000289
    declare @sun_true_long float = @sun_eq_of_ctr + @geom_mean_long_sun
    declare @sun_app_long float = @sun_true_long - 0.00569 - 0.00478*sin(radians(125.04 - 1934.136*@julian_century))
    declare @mean_obliq_ecliptic float = 23.0 + (26.0 + ((21.448 - @julian_century*(46.815 + @julian_century*(0.00059 - @julian_century*0.001813))))/60.0)/60.0
    declare @obliq_corr float = @mean_obliq_ecliptic + 0.00256*cos(radians(125.04 - 1934.136*@julian_century))
    declare @sun_declin float = degrees(asin(sin(radians(@obliq_corr))*sin(radians(@sun_app_long))))
    declare @var_y float = tan(radians(0.5*@obliq_corr))*tan(radians(0.5*@obliq_corr))
    declare @eq_of_time float = 4.0*degrees(@var_y*sin(2.0*radians(@geom_mean_long_sun)) - 2.0*@eccent_earth_orbit*sin(radians(@geom_mean_anom_sun)) + 4.0*@eccent_earth_orbit*@var_y*sin(radians(@geom_mean_anom_sun))*cos(2.0*radians(@geom_mean_long_sun)) - 0.5*@var_y*@var_y*sin(4.0*radians(@geom_mean_long_sun)) - 1.25*@eccent_earth_orbit*@eccent_earth_orbit*sin(2.0*radians(@geom_mean_anom_sun)))
    declare @ha_sunrise float = degrees(acos(cos(radians(90.833))/(cos(radians(@latitude))*cos(radians(@sun_declin)))-tan(radians(@latitude))*tan(radians(@sun_declin))))
    declare @solar_noon float = (720.0 - 4.0*@longitude - @eq_of_time + @timezone*60.0)/1440.0
    declare @sunrise time = convert(time, format(convert(datetime, (@solar_noon*1440.0 - @ha_sunrise*4.0)/1440.0), 'HH:mm:ss.ms'))
    declare @sunset time = convert(time, format(convert(datetime, (@solar_noon*1440.0 + @ha_sunrise*4.0)/1440.0), 'HH:mm:ss.ms'))

    insert into @SUNRISEANDSETTIMES(Sunrise, Sunset)
    select
        dateadd(minute,
            datepart(minute, @sunrise) + 0.5 + datepart(second, @sunrise)/60.0,
            dateadd(hour, datepart(hour, @sunrise), cast('00:00' as time(4)))),
        dateadd(minute,
            datepart(minute, @sunset) + 0.5 + datepart(second, @sunset)/60.0,
            dateadd(hour, datepart(hour, @sunset), cast('00:00' as time(4))))

    return
END
GO

/*
    Convert @date to GMT to account for Daylight savings

    Tests:

        select
            Tests.datetimeval,
            dbo.f_DatetimeToGMT(Tests.datetimeval) as datetimegmt
        from (
            values
                (datetimefromparts(2023, 3, 25, 23, 59, 0, 0)),
                (datetimefromparts(2023, 3, 26, 0, 59, 59, 0)),
                (datetimefromparts(2023, 3, 26, 2, 0, 0, 0)),
                (datetimefromparts(2023, 10, 29, 1, 59, 59, 0)),
                (datetimefromparts(2023, 10, 29, 2, 0, 0, 0)),
                (datetimefromparts(2023, 10, 29, 2, 1, 0, 0)),
                (datetimefromparts(2023, 10, 29, 3, 0, 0, 0)),
                (datetimefromparts(2024, 3, 30, 23, 59, 0, 0)),
                (datetimefromparts(2024, 3, 31, 0, 59, 59, 0)),
                (datetimefromparts(2024, 3, 31, 2, 0, 0, 0)),
                (datetimefromparts(2024, 10, 27, 1, 59, 59, 0)),
                (datetimefromparts(2024, 10, 27, 2, 0, 0, 0)),
                (datetimefromparts(2024, 10, 27, 2, 1, 0, 0)),
                (datetimefromparts(2024, 10, 27, 3, 0, 0, 0))
        ) as Tests(datetimeval)
 */
CREATE FUNCTION [dbo].[f_DatetimeToGMT] (@date DATETIME)
RETURNS DATETIME
AS
BEGIN
    declare @DatetimeGMT datetime

    select
        @DatetimeGMT = dateadd(hour, iif(@date between DST.SummerTimeStart and DST.WinterTimeStart, -1, 0), @date)
    from (
        select
            dateadd(day, -datepart(weekday, SavingsOffsets.winter), SavingsOffsets.winter) as WinterTimeStart,
            dateadd(day, -datepart(weekday, SavingsOffsets.summer), SavingsOffsets.summer) as SummerTimeStart
        from (
            select
               datetimefromparts(year(@date), 11, 1, 1, 59, 59, 995) as winter,
               datetimefromparts(year(@date), 4, 1, 0, 59, 59, 995) as summer
        ) as SavingsOffsets
    ) as DST

    return @DatetimeGMT
END
GO


/*
    Given a start year and a year count, list all the standard bank holidays for that count of years.

    select * from dbo.ft_BankHolidays(2019, 5) will give:
 ____________________________________________________________________________________________________________________________________________________________________________________________________
 | YearNo | NewYearsDay | GoodFriday | EasterMonday | MaydayBankHoliday | SpringBankHoliday | AugustBankHoliday | ChristmasDayHoliday | BoxingDayHoliday | SpecialBankHoliday | SpecialBankHoliday2 |
 ____________________________________________________________________________________________________________________________________________________________________________________________________
 | 2019   | 2019-01-01  | 2019-04-19 | 2019-04-22   | 2019-05-06        | 2019-05-27        | 2019-08-26        | 2019-12-25          | 2019-12-26       |                    |                     |
 | 2020   | 2020-01-01  | 2020-04-10 | 2020-04-13   | 2020-05-08        | 2020-05-25        | 2020-08-31        | 2020-12-25          | 2020-12-28       |                    |                     |
 | 2021   | 2021-01-01  | 2021-04-02 | 2021-04-05   | 2021-05-03        | 2021-05-31        | 2021-08-30        | 2021-12-27          | 2021-12-28       |                    |                     |
 | 2022   | 2022-01-03  | 2022-04-15 | 2022-04-18   | 2022-05-02        | 2022-06-02        | 2022-08-29        | 2022-12-27          | 2022-12-26       | 2022-06-03         | 2022-09-19          |
 | 2023   | 2023-01-02  | 2023-04-07 | 2023-04-10   | 2023-05-01        | 2023-05-29        | 2023-08-28        | 2023-12-25          | 2023-12-26       | 2023-05-08         |                     |
 ____________________________________________________________________________________________________________________________________________________________________________________________________
 */
CREATE FUNCTION [dbo].[ft_BankHolidays](
    @StartYear AS SMALLINT,
    @CountYears AS SMALLINT
)
RETURNS TABLE
AS RETURN (
    select
        yr.Number as YearNo,
        newyear.[Date] as NewYearsDay,
        easter.GoodFriday,
        easter.EasterMonday,
        mayday.[Date] as MaydayBankHoliday,
        springbh.[Date] as SpringBankHoliday,
        augbh.[Date] as AugustBankHoliday,
        christmas.[Date] as ChristmasDayHoliday,
        boxing.[Date] as BoxingDayHoliday,
        specialbh.[Date] as SpecialBankHoliday,
        specialbh2.[Date] as SpecialBankHoliday2
    from dbo.ft_Iota(@CountYears) iota
    cross apply (
        select
            @StartYear - 1 + iota.Number as [Number]
    ) as yr
    cross apply (
        select
            dbo.f_EasterSunday(yr.Number) as [EasterSunday]
    ) as step1
    cross apply (
        select
            dateadd(day, 1, step1.EasterSunday) as [EasterMonday],
            dateadd(day, -2, step1.EasterSunday) as [GoodFriday]
    ) easter
    cross apply (
        select
            datepart(weekday, '2019-08-23') as [Friday],
            datepart(weekday, '2019-08-24') as [Saturday],
            datepart(weekday, '2019-08-25') as [Sunday],
            datepart(weekday, '2019-08-26') as [Monday]
    ) const
    cross apply (
        select
            convert(date, str(yr.Number) + '-01-01', 121) as [Jan1st],
            convert(date, str(yr.Number) + '-05-01', 121) as [May1st],
            convert(date, str(yr.Number) + '-05-31', 121) as [May31],
            convert(date, str(yr.Number) + '-08-31', 121) as [Aug31],
            convert(date, str(yr.Number) + '-12-25', 121) as [Dec25th],
            convert(date, str(yr.Number) + '-12-26', 121) as [Dec26th]
    ) dat
    cross apply (
        select
            case datepart(weekday, dat.Jan1st)
                when const.Saturday then convert(date, str(yr.Number) + '-01-03', 121)
                when const.Sunday then convert(date, str(yr.Number) + '-01-02', 121)
                else dat.Jan1st
            end as [Date]
    ) newyear
    cross apply (
        select
            case yr.Number
                when 2020 then convert(date, '2020-05-08', 121)
                else dateadd(day, (7 + const.Monday - datepart(weekday, dat.May1st)) % 7, dat.May1st)
            end as [Date]
    ) mayday
    cross apply (
        select
            case yr.Number
                when 2022 then convert(date, '2022-06-02', 121)
                else dateadd(day,
                             iif(const.Monday > datepart(weekday, dat.May31), const.Monday - datepart(weekday, dat.May31) - const.Saturday, const.Monday - DATEPART(weekday, dat.May31)),
                             dat.May31)
            end as [Date]
    ) springbh
    cross apply (
        select
            case yr.Number
                when 2022 then convert(date, '2022-06-03', 121)
                when 2023 then convert(date, '2023-05-08', 121)
                else null
            end as [Date]
    ) specialbh
    cross apply (
        select
            case yr.Number
                when 2022 then convert(date, '2022-09-19', 121)
                else null
            end as [Date]
    ) specialbh2
    cross apply (
        select
            dateadd(day,
                iif(const.Monday > datepart(weekday, dat.Aug31), const.Monday - datepart(weekday, dat.Aug31) - const.Saturday, const.Monday - datepart(weekday, dat.Aug31)),
                dat.Aug31) as [Date]
    ) augbh
    cross apply (
        select
            case datepart(weekday, dat.Dec25th)
                when const.Saturday then dateadd(day, 2, dat.Dec25th)
                when const.Sunday then dateadd(day, 2, dat.Dec25th)
                else dat.Dec25th
            end as [Date]
    ) christmas
    cross apply (
        select
            case datepart(weekday, dat.Dec25th)
                when const.Friday then dateadd(day, 2, dat.Dec26th)
                when const.Saturday then dateadd(day, 2, dat.Dec26th)
                else dat.Dec26th
            end as [Date]
    ) boxing
)
GO

/**
 *  List all the calendar dates for a given start year and year count, with various attributes of those dates. This select statement
 *  generates a calendar for the years 2015 to 7 years after the current year.
 *  The columns in the output are as follows:
 *      - FullDateAlternateKey:     - the date of type DATE
 *      - FullDate and End Time:    - the date with time set to 23:59:59
 *      - Year, Month, Day:         - the year, month and day of the date
 *      - Days in Month:            - the number of days in the month of the date
 *      - MC Dates:                 - the first day of the month of the date
 *      - ME Dates:                 - the last day of the month of the date
 *      - ME Dates Text:            - the last day of the month of the date in 'MMM yyyy' format
 *      - WC Dates:                 - the first day of the week (Monday) for the date
 *      - WE Dates:                 - the last day of the week (Sunday) for the date
 *      - WEDates (Friday):         - the Friday of the week for the date
 *      - WE Dates plus End Time:   - the last day of the week with time set to 23:59:59
 *      - WE Dates Text:            - the last day of the week in 'dd MMM yyyy' format
 *      - Day of Week:              - a number representing the day of week (0=Monday, 6=Sunday)
 *      - Day Name:                 - name of the day (e.g. 'Monday')
 *      - Month Name:               - name of the month (e.g. 'Jan')
 *      - Fiscal Year End:          - fiscal year end for that date (e.g. 2024 for dates from 1^{st} April 2023 to 31^{st} March 2024)
 *      - Fiscal Month:             - fiscal month number (1..12)
 *      - Fiscal Quarter No:        - fiscal quarter number (1..4)
 *      - Fiscal Qtr:               - fiscal quarter in 'Qtr N' format
 *      - Fiscal Years:             - fiscal year range in 'YY-YY' format (e.g. '23-24' for fiscal year 2024)
 *      - UK Holiday:               - name of UK bank holiday if that date is a bank holiday, otherwise null
 *      - Non working day Type:     - 'Weekend' if that date is a weekend, 'Public Holiday' if that date is a public holiday, otherwise null
 *      - Workday Type:             - 0 if that date is a non-working day, 1 if it is a working day
 *      - Day Type:                 - 'NonWorkDay' if that date is a non-working day, 'WorkDay' if it is a working day
 *      - Sunrise:                  - sunrise time for that date at a specific location in Shrewsbury, UK (52.7073 N, 2.7553 W)
 *      - Sunset:                   - sunset time for that date at a specific location in Shrewsbury, UK (52.7073 N, 2.7553 W)
 *      - Moon Phase %:             - the percentage of the moon that is illuminated on that date (0% = new moon, 100% = full moon)
 **/
select
    Calendar.*
from (
    select
        p.StartYear,
        p.EndYear,
        YearCount = p.EndYear - p.StartYear + 1,
        CountDays = datediff(day, datefromparts(p.StartYear,1,1), datefromparts(p.EndYear,12,31)) + 1,
        Saturday = datepart(weekday, datefromparts(2019,11,30)),
        Sunday = datepart(weekday, datefromparts(2019,12,1))
    from (
        select
            StartYear = 2015,
            EndYear = year(sysdatetime()) + 7 -- 2031
    ) as p
) as Params
cross apply (
    select
        Cal.FullDateAlternateKey,
        datetimefromparts(Cal.[Year], Cal.[Month], Cal.[Day], 23, 59, 59, 0) as [FullDate and End Time],
        Cal.[Year],
        Cal.[Month],
        Cal.[Day],
        Cal.[Days in Month],
        datetimefromparts(Cal.[Year], Cal.[Month], 1, 0, 0, 0, 0) as [MC Dates],
        datetimefromparts(Cal.[Year], Cal.[Month], Cal.[Days in Month], 23, 59, 59, 0) as [ME Dates],
        left(format(datefromparts(Cal.[Year], Cal.[Month], Cal.[Days in Month]), 'MMM yyyy'), 8) as [ME Dates Text],
        Cal.[WC Dates],
        Cal.[WE Dates],
        Cal.[WEDates (Friday)],
        datetimefromparts(year(Cal.[WE Dates]), month(Cal.[WE Dates]), day(Cal.[WE Dates]), 23, 59, 59, 0) as [WE Dates plus End Time],
        left(format(Cal.[WE Dates], 'dd MMM yyyy'), 11) as [WE Dates Text],
        Cal.[Day of Week],
        Cal.[Day Name],
        Cal.[Month Name],
        Cal.[Fiscal Year End],
        Cal.[Fiscal Month],
        Cal.[Fiscal Quarter No],
        Cal.[Fiscal Qtr],
        concat(Cal.[Fiscal Year End]%100 - 1, '-', Cal.[Fiscal Year End]%100) as [Fiscal Years],
        Cal.[UK Holiday],
        iif(Cal.[IsWeekend] = 1, 'Weekend', iif(Cal.[IsHoliday] = 1, 'Public Holiday', null)) as [Non working day Type],
        iif(Cal.[IsWeekend] = 1 or Cal.[IsHoliday] = 1, 0, 1) as [Workday Type],
        iif(Cal.[IsWeekend] = 1 or Cal.[IsHoliday] = 1, 'NonWorkDay', 'WorkDay') as [Day Type],
        Cal.[Sunrise],
        Cal.[Sunset],
        Cal.[Moon Phase %]
    from (
        select
            rw.[Date] as FullDateAlternateKey,
            rw.[Year],
            rw.[Month],
            rw.[Day],
            day(eomonth(rw.[Date])) as [Days in Month],
            dateadd(day, -((datepart(weekday, rw.[Date]) + @@DATEFIRST + 5) % 7), rw.[Date]) as [WC Dates],
            dateadd(day, 6 - ((datepart(weekday, rw.[Date]) + @@DATEFIRST + 5) % 7), rw.[Date]) as [WE Dates],
            dateadd(day, 4 - ((datepart(weekday, rw.[Date]) + @@DATEFIRST + 5) % 7), rw.[Date]) as [WEDates (Friday)],
            (datepart(weekday, rw.[Date]) + @@DATEFIRST + 5) % 7 as [Day of Week],
            rw.[WeekDayName] as [Day Name],
            left(format(rw.[Date], 'MMM'), 3) as [Month Name],
            case
                when rw.[Month] > 3 then rw.[Year] + 1
                else rw.[Year]
            end as [Fiscal Year End],
            ((rw.[Month] + 8) % 12) + 1 as [Fiscal Month],
            ((rw.[Month] + 8) % 12)/3 + 1 as [Fiscal Quarter No],
            concat('Qtr ', ((rw.[Month] + 8) % 12)/3 + 1) as [Fiscal Qtr],
            dbo.f_FormatDate(rw.[Date], 'DD') as [DaySuffix],
            rw.[WeekDay],
            case rw.[WeekDay]
                when Params.Saturday then 1
                when Params.Sunday then 1
                else 0
            end as [IsWeekEnd],
            case rw.[WeekDay]
                when Params.Saturday then 0
                when Params.Sunday then 0
                else iif(dbo.f_IsWorkingDay(rw.[Date]) = 'Y', 0, 1)
            end as [IsHoliday],
            case
                when bh1.AugustBankHoliday is not null then 'August Bank Holiday'
                when bh2.BoxingDayHoliday is not null then iif(rw.[Month] = 12 and rw.[Day] = 26, 'Boxing Day', 'Bank Holiday in Lieu of Boxing Day')
                when bh3.ChristmasDayHoliday is not null then iif(rw.[Month] = 12 and rw.[Day] = 25, 'Christmas Day', 'Bank Holiday in Lieu of Christmas Day')
                when bh4.EasterMonday is not null then 'Easter Monday'
                when bh5.GoodFriday is not null then 'Good Friday'
                when bh6.MaydayBankHoliday is not null then iif(rw.[Year] = 2020, '75th anniversary of Victory in Europe (VE Day)', 'May Day Bank Holiday')
                when bh7.NewYearsDay is not null then iif(rw.[Month] = 1 and rw.[Day] = 1, 'New Years Day', 'Bank Holiday in Lieu of New Years Day')
                when bh8.SpringBankHoliday is not null then 'Spring Bank Holiday'
                when bh9.SpecialBankHoliday is not null then iif(rw.[Year] = 2023, 'Coronation of King Charles III', iif(rw.[Year] = 2022, 'Platinum Jubilee Bank Holiday', 'Special Bank Holiday'))
                when bh10.SpecialBankHoliday2 is not null then iif(rw.[Year] = 2022, 'State Funeral of Queen Elizabeth II', 'Special Bank Holiday 2')
            end as [UK Holiday],
            datename(month, rw.[Date]) as [MonthName],
            sun.Sunrise,
            sun.Sunset,
            cast(100.0 - abs(rw.NewMoons - floor(rw.NewMoons) - 0.5)*200.0 + 0.5 as int) as [Moon Phase %]
        from dbo.ft_Iota(Params.CountDays) iota
        cross apply (
            select
                r.[Date],
                datename(weekday, r.[Date]) as [WeekDayName],
                datepart(weekday, r.[Date]) as [WeekDay],
                day(r.[Date]) as [Day],
                month(r.[Date]) as [Month],
                year(r.[Date]) as [Year],
                datediff(day, datefromparts(2000,1,6), r.[Date])/29.53 as NewMoons
            from (
                select
                    dateadd(day, iota.Number, datefromparts(Params.StartYear-1,12,31)) as [Date]
            ) as r
        ) as rw
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh1
          on bh1.AugustBankHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh2
          on bh2.BoxingDayHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh3
          on bh3.ChristmasDayHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh4
          on bh4.EasterMonday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh5
          on bh5.GoodFriday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh6
          on bh6.MaydayBankHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh7
          on bh7.NewYearsDay = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh8
          on bh8.SpringBankHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh9
          on bh9.SpecialBankHoliday = rw.[Date]
        left outer join dbo.ft_BankHolidays(Params.StartYear, Params.YearCount) as bh10
          on bh10.SpecialBankHoliday2 = rw.[Date]
        cross apply dbo.ft_SunriseAndSetTimes(rw.[Date]) as sun
    ) as Cal
) as Calendar
go
