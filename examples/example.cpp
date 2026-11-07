#include "../dasmig/usergen.hpp"
#include <cstdint>
#include <iostream>

int main()
{
    // The singleton loads the lite tier (compiled in, or from resources/); the
    // libraries it composes load theirs the same way.
    auto& gen = dasmig::usg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the user data.\n";
        return 1;
    }
    std::cout << gen.supported_countries().size() << " countries have data in every part.\n\n";

    std::cout << "Random:\n";
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "  " << gen.get_user(dasmig::user_request{}) << '\n';
    }

    std::cout << "\nA Brazilian woman aged 25 to 40, with contacts that reach no one:\n";
    const dasmig::user_request request{.country = "BR",
                                       .person_sex = dasmig::sex::female,
                                       .ages = {.min = 25, .max = 40},
                                       .safe = true};
    const auto u = gen.get_user(request, std::uint64_t{42});
    std::cout << "  " << u << '\n'
              << "  phone:    " << u.mobile.international() << '\n'
              << "  address:  " << u.location.formatted() << '\n'
              << "  password: " << u.account.password << '\n'
              << "  signed up " << u.account.created << ", last login "
              << (u.account.last_login.empty() ? "never" : u.account.last_login) << ", " << u.account.status
              << " on " << u.account.platform_label << ", " << u.account.locale << ", "
              << u.account.timezone << ", " << u.account.currency << '\n';

    std::cout << "\nThe same person's papers and wallet, as safe test data:\n";
    dasmig::user_request held = request;
    dasmig::finance_request money;
    money.cards = {dasmig::card_kind::debit};
    held.identity = dasmig::identity_request{};
    held.finance = money;
    const auto full = gen.get_user(held, std::uint64_t{42});
    for (const auto& number : full.papers->numbers) std::cout << "  " << number.scheme << " " << number.formatted << '\n';
    for (const auto& account : full.wallet->accounts) std::cout << "  " << account.scheme << " " << account.formatted << '\n';
    for (const auto& card : full.wallet->cards) std::cout << "  " << card.network << " " << card.formatted << " (" << card.test_source << ")\n";

    std::cout << "\nThe same seed gives the same user:\n"
              << "  " << (gen.get_user(request, std::uint64_t{42}).to_json() == u.to_json() ? "equal" : "different")
              << '\n';

    std::cout << "\nTen users with a short numeric PIN as the password:\n";
    dasmig::user_request pin;
    pin.password = {.min_length = 4, .max_length = 4, .lowercase = false, .uppercase = false, .symbols = false};
    for (const auto& v : gen.get_user_batch(pin, {.count = 10}, std::uint64_t{7}))
    {
        std::cout << "  " << v.account.username << "  " << v.account.password << '\n';
    }
    return 0;
}
