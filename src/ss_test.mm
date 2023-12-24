// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#import <XCTest/XCTest.h>

extern "C" int xc_main();

@interface YassTests : XCTestCase

@end

@implementation YassTests

- (void)testGtest {
   XCTAssertEqual(0, xc_main());
}

@end


