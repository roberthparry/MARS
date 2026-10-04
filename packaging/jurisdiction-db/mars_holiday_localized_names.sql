-- Supplementary display names; these do not change holiday dates or policies.
-- Welsh terminology: https://www.gov.uk/gwyliau-banc

-- Exact event labels preserve the meaning of substitutions and one-off names.
create table if not exists holiday_event_localized_name (
    holiday_id integer not null references holiday_definition_entity(holiday_id),
    locale text not null,
    source_name text not null,
    localized_name text not null,
    primary key (holiday_id, locale, source_name)
);

with translations(holiday_key, name) as (
    values
        ('new_years_day', 'Dydd Calan'),
        ('good_friday', 'Gwener y Groglith'),
        ('easter_monday', 'Llun y Pasg'),
        ('may_day_bank_holiday', 'Gŵyl banc dechrau Mai'),
        ('spring_bank_holiday', 'Gŵyl banc y gwanwyn'),
        ('platinum_jubilee_bank_holiday', 'Gŵyl banc Jiwbilî Blatinwm'),
        ('state_funeral_qe2', 'Gŵyl y Banc ar gyfer Angladd Gwladol y Frenhines Elizabeth II'),
        ('coronation_king_charles_iii', 'Gŵyl y banc ar gyfer coroni Brenin Siarl III'),
        ('august_bank_holiday', 'Gŵyl banc yr haf'),
        ('christmas_day', 'Dydd Nadolig'),
        ('boxing_day', 'Dydd San Steffan')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select d.holiday_id, 'cy_GB', t.name, 'N'
from holiday_definition as d join translations as t using (holiday_key)
where d.jurisdiction_id in ('GB-ENG', 'GB-WLS')
  and not exists (select 1 from holiday_name as n where n.holiday_id = d.holiday_id and n.locale = 'cy_GB');

with translations(holiday_key, source_name, name) as (
    values
        ('new_years_day', 'Bank Holiday in Lieu of New Years Day', 'Dydd Calan (diwrnod amgen)'),
        ('christmas_day', 'Bank Holiday in Lieu of Christmas Day', 'Dydd Nadolig (diwrnod amgen)'),
        ('boxing_day', 'Bank Holiday in Lieu of Boxing Day', 'Dydd San Steffan (diwrnod amgen)'),
        ('may_day_bank_holiday', '75th anniversary of Victory in Europe (VE Day)',
         'Gŵyl banc dechrau Mai (diwrnod VE)')
)
insert or ignore into holiday_event_localized_name
select d.holiday_id, 'cy_GB', t.source_name, t.name
from holiday_definition as d join translations as t using (holiday_key)
where d.jurisdiction_id in ('GB-ENG', 'GB-WLS');

-- Irish holiday vocabulary:
-- https://workplacerelations.ie/ga/eolas-rithabhachtach/saoiri-poibli/
-- https://www.teanglann.ie/en/eid/Luan
-- UK-only event names below are application translations, not Irish observance rules.
-- Store common Irish labels once under ga, shared by ga_GB and ga_IE.
with translations(holiday_key, name) as (
    values
        ('new_years_day', 'Lá Caille'),
        ('new_year_holiday', 'Lá saoire na Bliana Nua'),
        ('saint_brigids_day', 'Lá Fhéile Bríde'),
        ('saint_patricks_day', 'Lá Fhéile Pádraig'),
        ('good_friday', 'Aoine an Chéasta'),
        ('easter_monday', 'Luan Cásca'),
        ('may_day', 'Lá saoire bainc na Bealtaine'),
        ('may_day_bank_holiday', 'Lá saoire bainc na Bealtaine'),
        ('spring_bank_holiday', 'Lá saoire bainc an earraigh'),
        ('june_bank_holiday', 'Lá saoire bainc mhí an Mheithimh'),
        ('august_bank_holiday', 'Lá saoire bainc mhí Lúnasa'),
        ('summer_bank_holiday', 'Lá saoire bainc an tsamhraidh'),
        ('october_bank_holiday', 'Lá saoire bainc mhí Dheireadh Fómhair'),
        ('saint_andrews_day', 'Lá Fhéile Aindriú'),
        ('christmas_day', 'Lá Nollag'),
        ('boxing_day', 'Lá Fhéile Stiofáin'),
        ('saint_stephens_day', 'Lá Fhéile Stiofáin'),
        ('whit_monday', 'Luan Cincíse'),
        ('platinum_jubilee_bank_holiday', 'Lá saoire bainc na hIubhaile Platanam'),
        ('state_funeral_qe2', 'Sochraid stáit na Banríona Eilís II'),
        ('coronation_king_charles_iii', 'Corónú an Rí Séarlas III')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select d.holiday_id, 'ga', t.name, 'N'
from holiday_definition as d join translations as t using (holiday_key)
where (d.jurisdiction_id = 'IE' or d.jurisdiction_id = 'GB' or d.jurisdiction_id glob 'GB-*')
  and not exists (select 1 from holiday_name as n where n.holiday_id = d.holiday_id and n.locale = 'ga');

with translations(holiday_key, source_name, name) as (
    values
        ('new_years_day', 'Bank Holiday in Lieu of New Years Day', 'Lá Caille (lá ionaid)'),
        ('christmas_day', 'Bank Holiday in Lieu of Christmas Day', 'Lá Nollag (lá ionaid)'),
        ('boxing_day', 'Bank Holiday in Lieu of Boxing Day', 'Lá Fhéile Stiofáin (lá ionaid)'),
        ('may_day_bank_holiday', '75th anniversary of Victory in Europe (VE Day)',
         '75 bliain ó Lá an Bhua san Eoraip'),
        ('may_day', '75th anniversary of Victory in Europe (VE Day)',
         '75 bliain ó Lá an Bhua san Eoraip')
)
insert or ignore into holiday_event_localized_name
select d.holiday_id, 'ga', t.source_name, t.name
from holiday_definition as d join translations as t using (holiday_key)
where d.jurisdiction_id = 'GB' or d.jurisdiction_id glob 'GB-*';

-- Western Frisian holiday names:
-- https://www.leeuwarden.nl/fy-nl/fergunningen/winkeltijdenontheffing-aanvragen/
-- https://www.fryslan.frl/fy/kontaktgegevens-provinsje-fryslan
-- https://weromrop.omropfryslan.nl/frl/sykje
with translations(holiday_key, name) as (
    values
        ('new_years_day', 'Nijjiersdei'),
        ('good_friday', 'Goed Freed'),
        ('easter_sunday', 'Earste Peaskedei'),
        ('easter_monday', 'Twadde Peaskedei'),
        ('kings_day', 'Keningsdei'),
        ('liberation_day', 'Befrijingsdei'),
        ('ascension_day', 'Himelfeartsdei'),
        ('whit_sunday', 'Earste Pinksterdei'),
        ('whit_monday', 'Twadde Pinksterdei'),
        ('christmas_day', 'Earste Krystdei'),
        ('second_christmas_day', 'Twadde Krystdei'),
        ('queens_day_wilhelmina', 'Keninginnedei'),
        ('queens_day_juliana', 'Keninginnedei'),
        ('queens_day_beatrix', 'Keninginnedei')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select d.holiday_id, 'fy', t.name, 'N'
from holiday_definition as d join translations as t using (holiday_key)
where d.jurisdiction_id = 'NL'
  and not exists (select 1 from holiday_name as n where n.holiday_id = d.holiday_id and n.locale = 'fy');

-- Canadian French terminology, shared by all French locale variants:
-- https://www.canada.ca/fr/immigration-refugies-citoyennete/organisation/publications-guides/decouvrir-canada/lisez-ligne/symboles-canadiens.html
-- https://www.canada.ca/fr/immigration-refugies-citoyennete/organisation/publications-guides/decouvrir-canada/lisez-ligne/histoire-canada.html
with translations(holiday_key, name) as (
    values
        ('new_year_s_day', 'Jour de l’An'),
        ('good_friday', 'Vendredi saint'),
        ('canada_day', 'Fête du Canada'),
        ('dominion_day', 'Fête du Dominion'),
        ('labour_day', 'Fête du Travail'),
        ('christmas_day', 'Noël')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select d.holiday_id, 'fr', t.name, 'N'
from holiday_definition as d join translations as t using (holiday_key)
where (d.jurisdiction_id = 'CA' or d.jurisdiction_id glob 'CA-*')
  and not exists (select 1 from holiday_name as n where n.holiday_id = d.holiday_id and n.locale = 'fr');

-- Correct mislabelled English source names without changing their text or primacy.
update holiday_name_locale set locale = 'en_CA'
where holiday_name_id in (
    select n.holiday_name_id from holiday_name as n
    join holiday_definition as d using (holiday_id)
    where d.jurisdiction_id = 'CA' and n.locale = 'ar' and n.is_primary = 'Y'
      and n.localized_name = d.default_name
      and n.localized_name in ('New Year''s Day', 'Good Friday', 'Canada Day',
                               'Dominion Day', 'Labour Day', 'Christmas Day')
);

-- Arabic terminology references:
-- https://www.nli.org.il/ar/discover/judaism/holidays
-- https://www.gov.il/ar/pages/pessah
-- https://m.knesset.gov.il/ar/about/pages/independence.aspx
-- Spanish terminology reference:
-- https://www.usa.gov/es/dias-festivos-estados-unidos
-- Yiddish/Ladino terminology references (supplementary display translations):
-- https://www.anschechesed.org/calendar-item/selichot/
-- https://kantoniko.com/words/ladino/azer

-- Correct the generated source's language tag, leaving its primary text intact.
update holiday_name_locale set locale = 'he_IL'
where holiday_name_id in (
    select names.holiday_name_id
    from holiday_name as names
    join holiday_definition as holiday using (holiday_id)
    where holiday.jurisdiction_id = 'IL' and names.locale = 'en_US'
      and names.localized_name = holiday.default_name
);

with translations(holiday_key, arabic_name, english_name) as (
    values
        ('holiday', 'يوم الاستقلال', 'Independence Day'),
        ('holiday_2', 'يوم الغفران', 'Yom Kippur'),
        ('holiday_3', 'عيد المظال', 'Sukkot'),
        ('holiday_4', 'عيد الفصح اليهودي', 'Passover'),
        ('holiday_5', 'رأس السنة العبرية', 'Rosh Hashanah'),
        ('holiday_6', 'عيد الأسابيع', 'Shavuot'),
        ('holiday_7', 'اليوم السابع من عيد الفصح اليهودي', 'Seventh day of Passover'),
        ('holiday_8', 'فرحة التوراة / شميني عتسيرت', 'Simchat Torah / Shemini Atzeret')
), languages(locale) as (values ('ar'), ('en'))
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, languages.locale,
       case languages.locale when 'ar' then translations.arabic_name else translations.english_name end, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id = 'IL'
                                 and holiday.holiday_key = translations.holiday_key
cross join languages
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = languages.locale
);

with translations(holiday_key, localized_name) as (
    values
        ('armistice_day', 'Día del Armisticio'),
        ('christmas_day', 'Día de Navidad'),
        ('columbus_day', 'Día de la Raza'),
        ('independence_day', 'Día de la Independencia'),
        ('juneteenth_national_independence_day', 'Día de la Liberación (Juneteenth)'),
        ('labor_day', 'Día del Trabajo'),
        ('martin_luther_king_jr_day', 'Día de Martin Luther King, Jr.'),
        ('memorial_day', 'Día de la Conmemoración de los Caídos'),
        ('new_year_s_day', 'Día de Año Nuevo'),
        ('thanksgiving_day', 'Día de Acción de Gracias'),
        ('veterans_day', 'Día de los Veteranos'),
        ('washington_s_birthday', 'Natalicio de George Washington')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, 'es', translations.localized_name, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id = 'US'
                                 and holiday.holiday_key = translations.holiday_key
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = 'es'
);

with translations(jurisdiction, holiday_key, yiddish_name, ladino_name) as (
    values
        ('IL', 'holiday', 'אומאָפּהענגיקייט־טאָג', 'Diya de la independensya'),
        ('IL', 'holiday_2', 'יום־כּיפּור', 'Yom Kipur'),
        ('IL', 'holiday_3', 'סוכּות', 'Sukot'),
        ('IL', 'holiday_4', 'פּסח', 'Pesah'),
        ('IL', 'holiday_5', 'ראש־השנה', 'Rosh Ashana'),
        ('IL', 'holiday_6', 'שבֿועות', 'Shavuot'),
        ('IL', 'holiday_7', 'שבֿיעי־של־פּסח', 'Shevii de Pesah'),
        ('IL', 'holiday_8', 'שׂמחת־תּורה / שמיני־עצרת', 'Simhat Tora / Shemini Atseret'),
        ('US', 'armistice_day', 'טאָג פֿונעם וואָפֿן־שטילשטאַנד', 'Diya del armistisyo'),
        ('US', 'christmas_day', 'קריסטמעס', 'Navidad'),
        ('US', 'columbus_day', 'קאָלומבוס־טאָג', 'Diya de Kristoval Kolon'),
        ('US', 'independence_day', 'אומאָפּהענגיקייט־טאָג', 'Diya de la independensya'),
        ('US', 'juneteenth_national_independence_day', 'דזשונטינט', 'Diya de la liberasyon (Juneteenth)'),
        ('US', 'labor_day', 'אַרבעטער־טאָג', 'Diya de los lavorantes'),
        ('US', 'martin_luther_king_jr_day', 'מאַרטין לוטער קינג דזשוניאָר־טאָג', 'Diya de Martin Luther King, Jr.'),
        ('US', 'memorial_day', 'אָנדענק־טאָג פֿאַר געפֿאַלענע סאָלדאַטן', 'Diya de rekuerdo de los soldados kayidos'),
        ('US', 'new_year_s_day', 'נײַיאָר־טאָג', 'Diya de anyo muevo'),
        ('US', 'thanksgiving_day', 'דאַנקזאָגונג־טאָג', 'Diya de agradesimiento'),
        ('US', 'veterans_day', 'וועטעראַנען־טאָג', 'Diya de los veteranos'),
        ('US', 'washington_s_birthday', 'דזשאָרדזש וואַשינגטאָנס געבוירן־טאָג', 'Kumpleanyos de George Washington'),
        ('PS', 'holiday', 'אַל־איסראַ און אַל־מיראַדזש', 'Isra i Miradj'),
        ('PS', 'holiday_2', 'אַל־איסראַ און אַל־מיראַדזש; מערבֿדיקער קריסטמעס', 'Isra i Miradj; Navidad oksidental'),
        ('PS', 'holiday_3', 'מוכאַמעדס געבוירן־טאָג', 'Nasyedura de Muhamed'),
        ('PS', 'holiday_4', 'מוכאַמעדס געבוירן־טאָג; אַרבעטער־טאָג', 'Nasyedura de Muhamed; Diya de los lavorantes'),
        ('PS', 'holiday_5', 'נײַיאָר־טאָג', 'Diya de anyo muevo'),
        ('PS', 'holiday_6', 'נײַיאָר־טאָג; עיד אַל־אַדחאַ', 'Diya de anyo muevo; Kurban Bayram'),
        ('PS', 'holiday_7', 'מוסולמענישער נײַיאָר', 'Anyo muevo musulmano'),
        ('PS', 'holiday_8', 'מוסולמענישער נײַיאָר; אומאָפּהענגיקייט־טאָג', 'Anyo muevo musulmano; Diya de la independensya'),
        ('PS', 'holiday_9', 'עיד אַל־אַדחאַ', 'Kurban Bayram'),
        ('PS', 'holiday_10', 'עיד אַל־אַדחאַ; אינטערנאַציאָנאַלער פֿרויען־טאָג', 'Kurban Bayram; Diya internasional de la mujer'),
        ('PS', 'holiday_11', 'אומאָפּהענגיקייט־טאָג', 'Diya de la independensya'),
        ('PS', 'holiday_12', 'אומאָפּהענגיקייט־טאָג; עיד אַל־פֿיטר', 'Diya de la independensya; Ramazan Bayram'),
        ('PS', 'holiday_13', 'אַרבעטער־טאָג', 'Diya de los lavorantes'),
        ('PS', 'holiday_14', 'אַרבעטער־טאָג; קריסטלעכער פּסחא', 'Diya de los lavorantes; Paskua kristiana'),
        ('PS', 'holiday_15', 'קריסטלעכער פּסחא', 'Paskua kristiana'),
        ('PS', 'holiday_16', 'עיד אַל־פֿיטר', 'Ramazan Bayram'),
        ('PS', 'holiday_17', 'מיזרחדיקער קריסטמעס', 'Navidad oriental'),
        ('PS', 'holiday_18', 'מערבֿדיקער קריסטמעס', 'Navidad oksidental'),
        ('PS', 'holiday_19', 'אינטערנאַציאָנאַלער פֿרויען־טאָג', 'Diya internasional de la mujer')
), languages(locale) as (values ('yi'), ('lad'))
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, languages.locale,
       case languages.locale when 'yi' then translations.yiddish_name else translations.ladino_name end, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id = translations.jurisdiction
                                 and holiday.holiday_key = translations.holiday_key
cross join languages
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = languages.locale
);

-- Hand-maintained US community-language labels, not changes to observance policy.
-- Pennsylvania Dutch common vocabulary: https://www.padutchdictionary.com/
-- Cajun French common vocabulary: LSU Department of French Studies.
with translations(holiday_key, cajun_name, pennsylvania_name) as (
    values
        ('armistice_day', 'Jour de l''Armistice', 'Waffeschtillschtandsdaag'),
        ('christmas_day', 'Noël', 'Grischtdaag'),
        ('columbus_day', 'Jour de Christophe Colomb', 'Kolumbusdaag'),
        ('independence_day', 'Fête de l''Indépendance', 'Der Viert vun Tschulei'),
        ('juneteenth_national_independence_day', 'Fête de la libération (Juneteenth)', 'Freiheetsdaag (Juneteenth)'),
        ('labor_day', 'Fête du Travail', 'Schaffersdaag'),
        ('martin_luther_king_jr_day', 'Jour de Martin Luther King, Jr.', 'Martin Luther King, Jr. Daag'),
        ('memorial_day', 'Jour du souvenir des soldats morts', 'Gedenkdaag fer die gfalle Soldaade'),
        ('new_year_s_day', 'Jour de l''An', 'Neiyaahrsdaag'),
        ('thanksgiving_day', 'Action de grâces', 'Dankfescht'),
        ('veterans_day', 'Jour des anciens combattants', 'Veteranedaag'),
        ('washington_s_birthday', 'Anniversaire de George Washington', 'George Washington sei Gebortsdaag')
), languages(locale) as (values ('frc'), ('pdc'))
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, languages.locale,
       case languages.locale when 'frc' then translations.cajun_name else translations.pennsylvania_name end, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id = 'US'
                                 and holiday.holiday_key = translations.holiday_key
cross join languages
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = languages.locale
);

-- Attested North Slope Iñupiaq names; untranslated holidays retain their source
-- text instead of guessed translations. https://inupiaq.tusaalanga.ca/glossary/english?showall=1
with translations(holiday_key, localized_name) as (
    values ('christmas_day', 'Kuraisimaġvik'), ('thanksgiving_day', 'Quyyavik')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, 'ik', translations.localized_name, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id = 'US'
                                 and holiday.holiday_key = translations.holiday_key
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = 'ik'
);

-- Hand-maintained English-based Patwa labels, not official holiday renamings.
-- https://jamaicanpatwah.com/b/talk-like-a-jamaican-how-to-say-the-weekdays-and-months
-- https://jamaicanpatwah.com/dictionary/user/anonymous/4765
with translations(holiday_key, localized_name) as (
    values
        ('ash_wednesday', 'Ash Wenzdeh'),
        ('boxing_day', 'Boxing Deh'),
        ('christmas_day', 'Krismuss Deh'),
        ('easter_monday', 'Easter Mondeh'),
        ('emancipation_day', 'Emancipation Deh'),
        ('good_friday', 'Gud Frideh'),
        ('independence_day', 'Independence Deh'),
        ('national_heroes_day', 'National Heroes Deh'),
        ('national_labour_day', 'National Labour Deh'),
        ('new_year_s_day', 'New Year Deh'),
        ('new_years_day', 'New Year Deh'),
        ('may_day_bank_holiday', 'May Deh Bank Holiday'),
        ('august_bank_holiday', 'Aagus Bank Holiday')
)
insert into holiday_name(holiday_id, locale, localized_name, is_primary)
select holiday.holiday_id, 'jam', translations.localized_name, 'N'
from translations
join holiday_definition as holiday on holiday.jurisdiction_id in ('JM', 'GB-ENG')
                                 and holiday.holiday_key = translations.holiday_key
where not exists (
    select 1 from holiday_name as existing
    where existing.holiday_id = holiday.holiday_id and existing.locale = 'jam'
);

-- Exact qualifiers preserve the distinction between an event and its observed day.
-- They apply only when the base holiday name also has a translation.
create table if not exists holiday_name_qualifier (
    locale text not null,
    source_suffix text not null,
    localized_suffix text not null,
    primary key (locale, source_suffix)
);
insert or ignore into holiday_name_qualifier values ('es', ' (observed)', ' (día de observancia)');
insert or ignore into holiday_name_qualifier values ('ga', ' (observed)', ' (lá ionaid)');
insert or ignore into holiday_name_qualifier values ('yi', ' (observed)', ' (פֿאַרלייגטער פֿײַערטאָג)');
insert or ignore into holiday_name_qualifier values ('lad', ' (observed)', ' (fiesta trasladada)');
insert or ignore into holiday_name_qualifier values ('frc', ' (observed)', ' (jour férié reporté)');
insert or ignore into holiday_name_qualifier values ('pdc', ' (observed)', ' (verschoowener Feierdaag)');
