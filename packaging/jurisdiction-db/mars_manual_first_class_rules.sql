-- Hand-maintained first-class holiday rules for Ukraine.
-- These close the post-2022 gap left by generated source coverage while
-- preserving the earlier materialised history already present in the package.

insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
values
    (2000001, 'UA', 'new_year', 'Новий рік', 'public', 'full_day', 'gregory', 1991, null,
     'Statutory New Year holiday in independent Ukraine.'),
    (2000002, 'UA', 'orthodox_christmas_jan7', 'Різдво Христове', 'public', 'full_day', 'gregory', 1991, 2022,
     'Christmas holiday observed on 7 January through 2022.'),
    (2000003, 'UA', 'christmas_dec25', 'Різдво Христове', 'public', 'full_day', 'gregory', 2017, null,
     'Christmas holiday observed on 25 December since its introduction in 2017.'),
    (2000004, 'UA', 'international_womens_day', 'Міжнародний жіночий день', 'public', 'full_day', 'gregory', 1991, null,
     'International Women’s Day.'),
    (2000005, 'UA', 'orthodox_easter', 'Великдень (Пасха)', 'public', 'full_day', 'gregory', 1991, null,
     'Orthodox Easter Sunday.'),
    (2000006, 'UA', 'trinity', 'Трійця', 'public', 'full_day', 'gregory', 1991, null,
     'Orthodox Pentecost / Trinity, 49 days after Orthodox Easter.'),
    (2000007, 'UA', 'workers_solidarity_day', 'День міжнародної солідарності трудящих', 'public', 'full_day', 'gregory', 1991, 2017,
     'Historic Labour Day name before the 2018 rename.'),
    (2000008, 'UA', 'labour_day', 'День праці', 'public', 'full_day', 'gregory', 2018, null,
     'Labour Day from the 2018 rename onward.'),
    (2000009, 'UA', 'victory_day', 'День Перемоги', 'public', 'full_day', 'gregory', 1991, 2015,
     'Victory Day name used through 2015.'),
    (2000010, 'UA', 'victory_day_memorial', 'День перемоги над нацизмом у Другій світовій війні (День перемоги)', 'public', 'full_day', 'gregory', 2016, 2022,
     'Victory/Remembrance holiday on 9 May for 2016-2022.'),
    (2000011, 'UA', 'remembrance_and_victory_day', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'full_day', 'gregory', 2023, null,
     'Remembrance and Victory Day observed on 8 May.'),
    (2000012, 'UA', 'constitution_day', 'День Конституції України', 'public', 'full_day', 'gregory', 1997, null,
     'Constitution Day.'),
    (2000013, 'UA', 'statehood_day_jul28', 'День Української Державності', 'public', 'full_day', 'gregory', 2022, 2023,
     'Statehood Day on 28 July in 2022-2023.'),
    (2000014, 'UA', 'statehood_day_jul15', 'День Української Державності', 'public', 'full_day', 'gregory', 2024, null,
     'Statehood Day moved to 15 July from 2024.'),
    (2000015, 'UA', 'independence_day_1991', 'День незалежності України', 'public', 'full_day', 'gregory', 1991, 1991,
     '1991 Independence Day observance on 16 July.'),
    (2000016, 'UA', 'independence_day', 'День Незалежності України', 'public', 'full_day', 'gregory', 1992, null,
     'Independence Day on 24 August.'),
    (2000017, 'UA', 'defender_of_ukraine_day', 'День захисника України', 'public', 'full_day', 'gregory', 2015, 2020,
     'Defender of Ukraine Day on 14 October.'),
    (2000018, 'UA', 'defenders_day_oct14', 'День захисників і захисниць України', 'public', 'full_day', 'gregory', 2021, 2022,
     'Defenders Day name after the 2021 rename, still on 14 October.'),
    (2000019, 'UA', 'defenders_day_oct1', 'День захисників і захисниць України', 'public', 'full_day', 'gregory', 2023, null,
     'Defenders Day moved to 1 October from 2023.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (2000001, 'uk-UA', 'Новий рік', 'Y'),
    (2000002, 'uk-UA', 'Різдво Христове', 'Y'),
    (2000003, 'uk-UA', 'Різдво Христове', 'Y'),
    (2000004, 'uk-UA', 'Міжнародний жіночий день', 'Y'),
    (2000005, 'uk-UA', 'Великдень (Пасха)', 'Y'),
    (2000006, 'uk-UA', 'Трійця', 'Y'),
    (2000007, 'uk-UA', 'День міжнародної солідарності трудящих', 'Y'),
    (2000008, 'uk-UA', 'День праці', 'Y'),
    (2000009, 'uk-UA', 'День Перемоги', 'Y'),
    (2000010, 'uk-UA', 'День перемоги над нацизмом у Другій світовій війні (День перемоги)', 'Y'),
    (2000011, 'uk-UA', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'Y'),
    (2000012, 'uk-UA', 'День Конституції України', 'Y'),
    (2000013, 'uk-UA', 'День Української Державності', 'Y'),
    (2000014, 'uk-UA', 'День Української Державності', 'Y'),
    (2000015, 'uk-UA', 'День незалежності України', 'Y'),
    (2000016, 'uk-UA', 'День Незалежності України', 'Y'),
    (2000017, 'uk-UA', 'День захисника України', 'Y'),
    (2000018, 'uk-UA', 'День захисників і захисниць України', 'Y'),
    (2000019, 'uk-UA', 'День захисників і захисниць України', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (2000001, 2000001, 1, 'fixed_date', 1, 1, null, 1991, null, 100, null),
    (2000002, 2000002, 1, 'fixed_date', 1, 7, null, 1991, 2022, 100, null),
    (2000003, 2000003, 1, 'fixed_date', 12, 25, null, 2017, null, 100, null),
    (2000004, 2000004, 1, 'fixed_date', 3, 8, null, 1991, null, 100, null),
    (2000005, 2000005, 1, 'orthodox_easter_offset', null, null, 0, 1991, null, 100, 'Orthodox Easter Sunday.'),
    (2000006, 2000006, 1, 'orthodox_easter_offset', null, null, 49, 1991, null, 100, 'Orthodox Trinity / Pentecost Sunday.'),
    (2000007, 2000007, 1, 'fixed_date', 5, 1, null, 1991, 2017, 100, 'Historic Labour Day name.'),
    (2000008, 2000008, 1, 'fixed_date', 5, 1, null, 2018, null, 100, 'Renamed Labour Day.'),
    (2000009, 2000009, 1, 'fixed_date', 5, 9, null, 1991, 2015, 100, 'Victory Day on 9 May.'),
    (2000010, 2000010, 1, 'fixed_date', 5, 9, null, 2016, 2022, 100, 'Victory/Remembrance holiday on 9 May.'),
    (2000011, 2000011, 1, 'fixed_date', 5, 8, null, 2023, null, 100, 'Remembrance and Victory Day on 8 May.'),
    (2000012, 2000012, 1, 'fixed_date', 6, 28, null, 1997, null, 100, null),
    (2000013, 2000013, 1, 'fixed_date', 7, 28, null, 2022, 2023, 100, 'Statehood Day before the 2024 move.'),
    (2000014, 2000014, 1, 'fixed_date', 7, 15, null, 2024, null, 100, 'Statehood Day after the move to 15 July.'),
    (2000015, 2000015, 1, 'fixed_date', 7, 16, null, 1991, 1991, 100, '1991 Independence Day date.'),
    (2000016, 2000016, 1, 'fixed_date', 8, 24, null, 1992, null, 100, null),
    (2000017, 2000017, 1, 'fixed_date', 10, 14, null, 2015, 2020, 100, 'Defender of Ukraine Day on 14 October.'),
    (2000018, 2000018, 1, 'fixed_date', 10, 14, null, 2021, 2022, 100, 'Defenders Day after rename, still on 14 October.'),
    (2000019, 2000019, 1, 'fixed_date', 10, 1, null, 2023, null, 100, 'Defenders Day moved to 1 October.');

-- Hand-maintained historical South Africa rules to extend coverage back to 1926.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
values
    (2100001, 'ZA', 'new_years_day_historic', 'New Year''s Day', 'public', 'full_day', 'gregory', 1926, 1994,
     'Historic New Year holiday before the post-1994 South African holiday regime.'),
    (2100002, 'ZA', 'easter_monday', 'Easter Monday', 'public', 'full_day', 'gregory', 1926, 1979,
     'Monday after Gregorian Easter Sunday under the earlier national holiday regime.'),
    (2100003, 'ZA', 'family_day_july10', 'Family Day', 'public', 'full_day', 'gregory', 1961, 1973,
     'Historic Family Day held on 10 July under the early Republic holiday regime.'),
    (2100004, 'ZA', 'van_riebeecks_day', 'Van Riebeeck''s Day', 'public', 'full_day', 'gregory', 1952, 1979,
     'Historic holiday on 6 April.'),
    (2100005, 'ZA', 'founders_day', 'Founders'' Day', 'public', 'full_day', 'gregory', 1980, 1994,
     'Historic renamed successor to Van Riebeeck''s Day.'),
    (2100006, 'ZA', 'workers_day_first_friday', 'Workers'' Day', 'public', 'full_day', 'gregory', 1987, 1989,
     'Historic Workers'' Day observed on the first Friday in May.'),
    (2100007, 'ZA', 'ascension_day', 'Ascension Day', 'public', 'full_day', 'gregory', 1926, 1993,
     'Historic Ascension Day holiday, 39 days after Easter Sunday.'),
    (2100008, 'ZA', 'empire_day', 'Empire Day', 'public', 'full_day', 'gregory', 1926, 1951,
     'Historic holiday on 24 May.'),
    (2100009, 'ZA', 'union_day', 'Union Day', 'public', 'full_day', 'gregory', 1926, 1960,
     'Historic holiday on 31 May before Republic Day.'),
    (2100010, 'ZA', 'republic_day', 'Republic Day', 'public', 'full_day', 'gregory', 1961, 1993,
     'Historic holiday on 31 May after the creation of the Republic.'),
    (2100011, 'ZA', 'queens_birthday', 'Queen''s Birthday', 'public', 'full_day', 'gregory', 1952, 1960,
     'Historic holiday on the second Monday in July.'),
    (2100012, 'ZA', 'kings_birthday', 'King''s Birthday', 'public', 'full_day', 'gregory', 1926, 1951,
     'Historic holiday on the first Monday in August.'),
    (2100013, 'ZA', 'settlers_day', 'Settlers'' Day', 'public', 'full_day', 'gregory', 1952, 1979,
     'Historic holiday on the first Monday in September.'),
    (2100014, 'ZA', 'kruger_day', 'Kruger Day', 'public', 'full_day', 'gregory', 1952, 1993,
     'Historic holiday on 10 October.'),
    (2100015, 'ZA', 'dingaans_day', 'Dingaan''s Day', 'public', 'full_day', 'gregory', 1926, 1951,
     'Historic holiday on 16 December before the 1952 renaming.'),
    (2100016, 'ZA', 'day_of_the_covenant', 'Day of the Covenant', 'public', 'full_day', 'gregory', 1952, 1979,
     'Historic successor name on 16 December from 1952.'),
    (2100017, 'ZA', 'day_of_the_vow', 'Day of the Vow', 'public', 'full_day', 'gregory', 1980, 1994,
     'Historic successor name on 16 December from 1980 until the post-1994 change.'),
    (2100018, 'ZA', 'boxing_day', 'Boxing Day', 'public', 'full_day', 'gregory', 1926, 1979,
     'Historic 26 December holiday before the Day of Goodwill rename.'),
    (2100019, 'ZA', 'family_day_easter', 'Family Day', 'public', 'full_day', 'gregory', 1980, 1994,
     'Monday after Gregorian Easter Sunday under the late pre-1995 holiday regime.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (2100001, 'en-ZA', 'New Year''s Day', 'Y'),
    (2100002, 'en-ZA', 'Easter Monday', 'Y'),
    (2100003, 'en-ZA', 'Family Day', 'Y'),
    (2100004, 'en-ZA', 'Van Riebeeck''s Day', 'Y'),
    (2100005, 'en-ZA', 'Founders'' Day', 'Y'),
    (2100006, 'en-ZA', 'Workers'' Day', 'Y'),
    (2100007, 'en-ZA', 'Ascension Day', 'Y'),
    (2100008, 'en-ZA', 'Empire Day', 'Y'),
    (2100009, 'en-ZA', 'Union Day', 'Y'),
    (2100010, 'en-ZA', 'Republic Day', 'Y'),
    (2100011, 'en-ZA', 'Queen''s Birthday', 'Y'),
    (2100012, 'en-ZA', 'King''s Birthday', 'Y'),
    (2100013, 'en-ZA', 'Settlers'' Day', 'Y'),
    (2100014, 'en-ZA', 'Kruger Day', 'Y'),
    (2100015, 'en-ZA', 'Dingaan''s Day', 'Y'),
    (2100016, 'en-ZA', 'Day of the Covenant', 'Y'),
    (2100017, 'en-ZA', 'Day of the Vow', 'Y'),
    (2100018, 'en-ZA', 'Boxing Day', 'Y'),
    (2100019, 'en-ZA', 'Family Day', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (2100001, 2100001, 1, 'fixed_date', 1, 1, null, null, null, 1926, 1994, 100, null),
    (2100002, 2100002, 1, 'easter_offset', null, null, null, null, 1, 1926, 1979, 100, 'Historic Easter Monday.'),
    (2100003, 2100003, 1, 'fixed_date', 7, 10, null, null, null, 1961, 1973, 100, 'Historic Family Day on 10 July.'),
    (2100004, 2100004, 1, 'fixed_date', 4, 6, null, null, null, 1952, 1979, 100, 'Historic Van Riebeeck''s Day on 6 April.'),
    (2100005, 2100005, 1, 'fixed_date', 4, 6, null, null, null, 1980, 1994, 100, 'Historic Founders'' Day on 6 April.'),
    (2100006, 2100006, 1, 'nth_weekday', 5, null, 5, 1, null, 1987, 1989, 100, 'Historic Workers'' Day on the first Friday in May.'),
    (2100007, 2100007, 1, 'easter_offset', null, null, null, null, 39, 1926, 1993, 100, 'Historic Ascension Day.'),
    (2100008, 2100008, 1, 'fixed_date', 5, 24, null, null, null, 1926, 1951, 100, 'Historic Empire Day.'),
    (2100009, 2100009, 1, 'fixed_date', 5, 31, null, null, null, 1926, 1960, 100, 'Historic Union Day.'),
    (2100010, 2100010, 1, 'fixed_date', 5, 31, null, null, null, 1961, 1993, 100, 'Historic Republic Day.'),
    (2100011, 2100011, 1, 'nth_weekday', 7, null, 1, 2, null, 1952, 1960, 100, 'Historic Queen''s Birthday on the second Monday in July.'),
    (2100012, 2100012, 1, 'nth_weekday', 8, null, 1, 1, null, 1926, 1951, 100, 'Historic King''s Birthday on the first Monday in August.'),
    (2100013, 2100013, 1, 'nth_weekday', 9, null, 1, 1, null, 1952, 1979, 100, 'Historic Settlers'' Day on the first Monday in September.'),
    (2100014, 2100014, 1, 'fixed_date', 10, 10, null, null, null, 1952, 1993, 100, 'Historic Kruger Day.'),
    (2100015, 2100015, 1, 'fixed_date', 12, 16, null, null, null, 1926, 1951, 100, 'Historic Dingaan''s Day.'),
    (2100016, 2100016, 1, 'fixed_date', 12, 16, null, null, null, 1952, 1979, 100, 'Historic Day of the Covenant.'),
    (2100017, 2100017, 1, 'fixed_date', 12, 16, null, null, null, 1980, 1994, 100, 'Historic Day of the Vow.'),
    (2100018, 2100018, 1, 'fixed_date', 12, 26, null, null, null, 1926, 1979, 100, 'Historic Boxing Day.'),
    (2100019, 2100019, 1, 'easter_offset', null, null, null, null, 1, 1980, 1994, 100, 'Historic Family Day on Easter Monday.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    source_document_id,
    role
) values
    (2100001, 2100001, 15, 'definition'),
    (2100002, 2100002, 15, 'definition'),
    (2100003, 2100003, 15, 'definition'),
    (2100004, 2100004, 15, 'definition'),
    (2100005, 2100005, 15, 'definition'),
    (2100006, 2100006, 15, 'definition'),
    (2100007, 2100007, 15, 'definition'),
    (2100008, 2100008, 15, 'definition'),
    (2100009, 2100009, 15, 'definition'),
    (2100010, 2100010, 15, 'definition'),
    (2100011, 2100011, 15, 'definition'),
    (2100012, 2100012, 15, 'definition'),
    (2100013, 2100013, 15, 'definition'),
    (2100014, 2100014, 15, 'definition'),
    (2100015, 2100015, 15, 'definition'),
    (2100016, 2100016, 15, 'definition'),
    (2100017, 2100017, 15, 'definition'),
    (2100018, 2100018, 15, 'definition'),
    (2100019, 2100019, 15, 'definition');

-- Hand-maintained historical Netherlands royal holiday rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
values
    (2200001, 'NL', 'queens_day_wilhelmina', 'Koninginnedag', 'public', 'full_day', 'gregory', 1926, 1948,
     'Queen''s Day on 31 August during the reign of Wilhelmina.'),
    (2200002, 'NL', 'queens_day_juliana', 'Koninginnedag', 'public', 'full_day', 'gregory', 1949, 1979,
     'Queen''s Day on 30 April during the reign of Juliana, shifted forward when it fell on a Sunday.'),
    (2200003, 'NL', 'queens_day_beatrix', 'Koninginnedag', 'public', 'full_day', 'gregory', 1980, 2013,
     'Queen''s Day on 30 April during the reign of Beatrix, shifted backward when it fell on a Sunday.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (2200001, 'nl-NL', 'Koninginnedag', 'Y'),
    (2200002, 'nl-NL', 'Koninginnedag', 'Y'),
    (2200003, 'nl-NL', 'Koninginnedag', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (2200001, 2200001, 1, 'fixed_date', 8, 31, 1926, 1948, 100, 'Queen''s Day on 31 August during the Wilhelmina era.'),
    (2200002, 2200002, 1, 'fixed_date', 4, 30, 1949, 1979, 100, 'Queen''s Day on 30 April during the Juliana era.'),
    (2200003, 2200003, 1, 'fixed_date', 4, 30, 1980, 2013, 100, 'Queen''s Day on 30 April during the Beatrix era.');

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    weekend_mask,
    suppress_original,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (2200001, 2200002, 2200002, 'next_weekday', 'Koninginnedag', '7', 'Y', 1949, 1979, 100, 'During the Juliana era, a Sunday Queen''s Day moved to the next weekday.'),
    (2200002, 2200003, 2200003, 'previous_weekday', 'Koninginnedag', '7', 'Y', 1980, 2013, 100, 'During the Beatrix era, a Sunday Queen''s Day moved to the previous weekday.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    source_document_id,
    role
) values
    (2200001, 2200001, null, 17, 'definition'),
    (2200002, 2200002, 2200001, 17, 'observance'),
    (2200003, 2200003, 2200002, 17, 'observance');

-- Hand-maintained modern Scotland bank holiday rules.
insert into holiday_definition(
    holiday_id,
    jurisdiction_id,
    holiday_key,
    default_name,
    holiday_class,
    scope,
    calendar_system_id,
    valid_from_year,
    valid_to_year,
    notes
)
values
    (2300001, 'GB-SCT', 'new_years_day', 'New Year''s Day', 'bank', 'full_day', 'gregory', 1974, null,
     'Scottish New Year bank holiday with substitute handling.'),
    (2300002, 'GB-SCT', 'new_year_holiday', 'New Year Holiday', 'bank', 'full_day', 'gregory', 1974, null,
     'Second Scottish New Year bank holiday with substitute handling.'),
    (2300003, 'GB-SCT', 'good_friday', 'Good Friday', 'bank', 'full_day', 'gregory', 1974, null,
     'Good Friday is widely observed as a Scottish bank holiday.'),
    (2300004, 'GB-SCT', 'may_day', 'May Day', 'bank', 'full_day', 'gregory', 1978, null,
     'First Monday in May.'),
    (2300005, 'GB-SCT', 'spring_bank_holiday', 'Spring Bank Holiday', 'bank', 'full_day', 'gregory', 1971, null,
     'Last Monday in May.'),
    (2300006, 'GB-SCT', 'platinum_jubilee_bank_holiday', 'Platinum Jubilee Bank Holiday', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (2300007, 'GB-SCT', 'state_funeral_qe2', 'State Funeral of Queen Elizabeth II', 'special', 'full_day', 'gregory', 2022, 2022,
     'Additional one-off bank holiday.'),
    (2300008, 'GB-SCT', 'coronation_king_charles_iii', 'Coronation of King Charles III', 'special', 'full_day', 'gregory', 2023, 2023,
     'Additional one-off bank holiday.'),
    (2300009, 'GB-SCT', 'summer_bank_holiday', 'Summer Bank Holiday', 'bank', 'full_day', 'gregory', 1971, null,
     'First Monday in August in Scotland.'),
    (2300010, 'GB-SCT', 'saint_andrews_day', 'Saint Andrew''s Day', 'bank', 'full_day', 'gregory', 2007, null,
     'Saint Andrew''s Day with substitute handling where needed.'),
    (2300011, 'GB-SCT', 'christmas_day', 'Christmas Day', 'bank', 'full_day', 'gregory', 1974, null,
     'Christmas Day with paired substitute handling.'),
    (2300012, 'GB-SCT', 'boxing_day', 'Boxing Day', 'bank', 'full_day', 'gregory', 1974, null,
     'Boxing Day with paired substitute handling.');

insert into holiday_name(holiday_id, locale, localized_name, is_primary)
values
    (2300001, 'en-GB', 'New Year''s Day', 'Y'),
    (2300002, 'en-GB', 'New Year Holiday', 'Y'),
    (2300003, 'en-GB', 'Good Friday', 'Y'),
    (2300004, 'en-GB', 'May Day', 'Y'),
    (2300005, 'en-GB', 'Spring Bank Holiday', 'Y'),
    (2300006, 'en-GB', 'Platinum Jubilee Bank Holiday', 'Y'),
    (2300007, 'en-GB', 'State Funeral of Queen Elizabeth II', 'Y'),
    (2300008, 'en-GB', 'Coronation of King Charles III', 'Y'),
    (2300009, 'en-GB', 'Summer Bank Holiday', 'Y'),
    (2300010, 'en-GB', 'Saint Andrew''s Day', 'Y'),
    (2300011, 'en-GB', 'Christmas Day', 'Y'),
    (2300012, 'en-GB', 'Boxing Day', 'Y');

insert into holiday_rule(
    holiday_rule_id,
    holiday_id,
    sequence_no,
    rule_kind,
    month,
    day,
    weekday,
    ordinal,
    offset_days,
    holiday_date,
    valid_from_year,
    valid_to_year,
    priority,
    notes
) values
    (2300001, 2300001, 1, 'fixed_date', 1, 1, null, null, null, null, 1974, null, 10, null),
    (2300002, 2300002, 1, 'fixed_date', 1, 2, null, null, null, null, 1974, null, 20, null),
    (2300003, 2300003, 1, 'easter_offset', null, null, null, null, -2, null, 1974, null, 30, null),
    (2300004, 2300004, 1, 'nth_weekday', 5, null, 1, 1, null, null, 1978, null, 40, 'First Monday in May.'),
    (2300005, 2300005, 1, 'last_weekday', 5, null, 1, -1, null, null, 1971, null, 50, 'Last Monday in May.'),
    (2300006, 2300006, 1, 'one_off', null, null, null, null, null, '2022-06-03', 2022, 2022, 60, 'Additional bank holiday for the Platinum Jubilee.'),
    (2300007, 2300007, 1, 'one_off', null, null, null, null, null, '2022-09-19', 2022, 2022, 70, 'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (2300008, 2300008, 1, 'one_off', null, null, null, null, null, '2023-05-08', 2023, 2023, 80, 'Additional bank holiday for the Coronation of King Charles III.'),
    (2300009, 2300009, 1, 'nth_weekday', 8, null, 1, 1, null, null, 1971, null, 90, 'First Monday in August.'),
    (2300010, 2300010, 1, 'fixed_date', 11, 30, null, null, null, null, 2007, null, 100, null),
    (2300011, 2300011, 1, 'fixed_date', 12, 25, null, null, null, null, 1974, null, 110, null),
    (2300012, 2300012, 1, 'fixed_date', 12, 26, null, null, null, null, 1974, null, 120, null),
    (2300013, 2300004, 2, 'one_off', null, null, null, null, null, '2020-05-08', 2020, 2020, 5, 'VE Day exception date represented as a high-priority override candidate.'),
    (2300014, 2300005, 2, 'one_off', null, null, null, null, null, '2022-06-02', 2022, 2022, 5, 'Moved Spring Bank Holiday date for 2022.');

insert into holiday_observance_rule(
    holiday_observance_rule_id,
    holiday_id,
    holiday_rule_id,
    observed_rule_kind,
    observed_name,
    suppress_original,
    valid_from_year,
    priority,
    notes
) values
    (2300001, 2300001, 2300001, 'next_weekday', 'New Year''s Day (observed)', 'Y', 1974, 10, 'If 1 January falls on the ordinary Scottish weekend, observe on the following weekday.'),
    (2300002, 2300002, 2300002, 'next_non_holiday', 'New Year Holiday (observed)', 'Y', 1974, 20, 'If 2 January falls on the ordinary Scottish weekend, observe on the next non-holiday weekday.'),
    (2300003, 2300010, 2300010, 'next_weekday', 'Saint Andrew''s Day (observed)', 'Y', 2007, 30, 'If Saint Andrew''s Day falls on the ordinary Scottish weekend, observe on the following weekday.'),
    (2300004, 2300011, 2300011, 'christmas_pair', 'Christmas Day (observed)', 'Y', 1974, 40, 'Christmas and Boxing Day substitutions interact and must be resolved together.'),
    (2300005, 2300012, 2300012, 'christmas_pair', 'Boxing Day (observed)', 'Y', 1974, 50, 'Christmas and Boxing Day substitutions interact and must be resolved together.');

insert into holiday_exception(
    holiday_exception_id,
    jurisdiction_id,
    holiday_id,
    holiday_rule_id,
    holiday_date,
    action,
    name,
    replacement_holiday_key,
    valid_from_year,
    valid_to_year,
    priority,
    source_document_id,
    notes
) values
    (2300001, 'GB-SCT', 2300004, 2300004, '2020-05-08', 'replace', '75th anniversary of Victory in Europe (VE Day)', 'may_day', 2020, 2020, 0, 7, 'May Day moved from Monday 2020-05-04 to Friday 2020-05-08.'),
    (2300002, 'GB-SCT', 2300005, 2300005, '2022-06-02', 'replace', 'Spring Bank Holiday', 'spring_bank_holiday', 2022, 2022, 0, 7, 'Spring Bank Holiday moved from Monday 2022-05-30 to Thursday 2022-06-02.'),
    (2300003, 'GB-SCT', 2300006, 2300006, '2022-06-03', 'add', 'Platinum Jubilee Bank Holiday', null, 2022, 2022, 0, 7, 'Additional bank holiday for the Platinum Jubilee.'),
    (2300004, 'GB-SCT', 2300007, 2300007, '2022-09-19', 'add', 'State Funeral of Queen Elizabeth II', null, 2022, 2022, 0, 7, 'Additional bank holiday for the State Funeral of Queen Elizabeth II.'),
    (2300005, 'GB-SCT', 2300008, 2300008, '2023-05-08', 'add', 'Coronation of King Charles III', null, 2023, 2023, 0, 7, 'Additional bank holiday for the Coronation of King Charles III.');

insert into holiday_rule_source(
    holiday_id,
    holiday_rule_id,
    holiday_observance_rule_id,
    holiday_exception_id,
    source_document_id,
    role
) values
    (2300001, 2300001, 2300001, null, 7, 'observance'),
    (2300002, 2300002, 2300002, null, 7, 'observance'),
    (2300003, 2300003, null, null, 7, 'definition'),
    (2300004, 2300004, null, 2300001, 7, 'exception'),
    (2300005, 2300005, null, 2300002, 7, 'exception'),
    (2300006, 2300006, null, 2300003, 7, 'exception'),
    (2300007, 2300007, null, 2300004, 7, 'exception'),
    (2300008, 2300008, null, 2300005, 7, 'exception'),
    (2300009, 2300009, null, null, 7, 'definition'),
    (2300010, 2300010, 2300003, null, 7, 'observance'),
    (2300011, 2300011, 2300004, null, 7, 'observance'),
    (2300012, 2300012, 2300005, null, 7, 'observance');
