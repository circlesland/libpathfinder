#include "flow.h"
#include "binaryImporter.h"

#include <iostream>
#include <sstream>
#include "log.h"
#include "types.h"

using namespace std;

DB db;
extern "C"
{

size_t loadDbFromFile(char const *_filename) {
    log_debug("-> loadDB(_filename: '%s')", _filename);
    ifstream input_file(_filename);
    if (!input_file.is_open()) {
        log_error("Could not open '%s'", _filename);
        return 0;
    }

    size_t blockNumber{};
    tie(blockNumber, db) = BinaryImporter(input_file).readBlockNumberAndDB();

    input_file.close();
    log_debug("<- loadDB(_filename: '%s')", _filename);

    return blockNumber;
}

size_t loadDB(char const *_data, size_t _length) {
    log_debug("-> loadDB(data: ..., _length: '%li')", _length);

    string data(_data, _length);
    istringstream stream(data);

    size_t blockNumber{};
    tie(blockNumber, db) = BinaryImporter(stream).readBlockNumberAndDB();

    log_debug("<- loadDB(data: ..., _length: '%li')", _length);
    return blockNumber;
}

Flow computeFlow(
        Address const &_source,
        Address const &_sink,
        Int const &_value
) {
    log_debug("-> computeFlow(source:'%s', sink: '%s', value: %s)", to_string(_source).c_str(), to_string(_sink).c_str(), to_string(_value).c_str());
    log_debug("   computeFlow(source:'%s', sink: '%s', value: %s): Total edge count: %li", to_string(_source).c_str(), to_string(_sink).c_str(), to_string(_value).c_str(), db.m_edges.size());

    auto[flow, transfers] = computeFlow(_source, _sink, db.edges(), _value);

    log_debug("   computeFlow(source:'%s', sink: '%s', value: %s): Max flow: %s", to_string(_source).c_str(), to_string(_sink).c_str(), to_string(_value).c_str(), to_string(flow).c_str());
    log_debug("<- computeFlow(source:'%s', sink: '%s', value: %s)", to_string(_source).c_str(), to_string(_sink).c_str(), to_string(_value).c_str());

    return Flow(flow, transfers);
}

size_t edgeCount() {
    log_debug("-* edgeCount()");
    return db.edges().size();
}

void delayEdgeUpdates() {
    log_info("-* delayEdgeUpdates()");
    db.delayEdgeUpdates();
}

void performEdgeUpdates() {
    log_info("-> performEdgeUpdates()");
    db.performEdgeUpdates();
    log_info("<- performEdgeUpdates()");
}

TrustRelation* adjacencies(string const& _user)
{
    log_debug("-> adjacencies(_user: '%s')", _user.c_str());

    Address user{string(_user)};

    auto v = new vector<TrustRelation>();
    for (auto const& [address, safe]: db.safes) {
        for (auto const&[sendTo, percentage]: safe.limitPercentage) {
            if (sendTo != address && (user == address || user == sendTo)) {
                auto a = TrustRelation(
                        sendTo,
                        user == sendTo ? address : user,
                        percentage);

                v->push_back(a);
            }
        }
    }

    log_debug("   adjacencies(_user: '%s'): Found %li adjacent nodes.", _user.c_str(), v->size());
    log_debug("<- adjacencies(_user: '%s')", _user.c_str());

    return v->data();
}


void signup(char const *_user, char const *_token) {
    log_debug("-* signup(_user: '%s', token: '%s')", &_user, &_token);
    db.signup(Address(string(_user)), Address(string(_token)));
}

void organizationSignup(char const *_organization) {
    log_debug("-* organizationSignup(_organization: '%s')", &_organization);
    db.organizationSignup(Address(string(_organization)));
}

void trust(char const *_canSendTo, char const *_user, int _limitPercentage) {
    log_debug("-* trust(_canSendTo: '%s', _user: '%s', _limitPercentage: %i)", &_canSendTo, &_user, _limitPercentage);
    db.trust(Address(string(_canSendTo)), Address(string(_user)), uint32_t(_limitPercentage));
}

void transfer(char const *_token, char const *_from, char const *_to, Int _value) {
    log_debug("-* transfer(_token: '%s', _from: '%s', _to: '%s', value: %s)", &_token, &_from, _to, to_string(_value).c_str());
    db.transfer(
            Address(string(_token)),
            Address(string(_from)),
            Address(string(_to)),
            _value
    );
}
}

int main() {
    loadDbFromFile("/home/daniel/src/circles-world/libpathfinder/db.dat");

    auto result1 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x4a9aFfA9249F36fd0629f342c182A4e94A13C2e0"),
            Int("999999999999999999999999"));

    auto result2 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x42cEDde51198D1773590311E2A340DC06B24cB37"),
            Int("999999999999999999999999"));

    auto result3 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x4a9aFfA9249F36fd0629f342c182A4e94A13C2e0"),
            Int("999999999999999999999999"));

    auto result4 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x42cEDde51198D1773590311E2A340DC06B24cB37"),
            Int("999999999999999999999999"));

    auto result5 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x4a9aFfA9249F36fd0629f342c182A4e94A13C2e0"),
            Int("999999999999999999999999"));

    auto result6 = computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x42cEDde51198D1773590311E2A340DC06B24cB37"),
            Int("999999999999999999999999"));

    if (result1.flow != result3.flow)
        log_error("Fuckup 1");
    if (result2.flow != result4.flow)
        log_error("Fuckup 2");
    if (result3.flow != result5.flow)
        log_error("Fuckup 3");
    if (result4.flow != result6.flow)
        log_error("Fuckup 4");


    return 0;
}