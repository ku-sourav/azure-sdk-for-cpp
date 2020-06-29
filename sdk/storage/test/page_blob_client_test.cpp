// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "page_blob_client_test.hpp"

namespace Azure { namespace Storage { namespace Test {

  std::shared_ptr<Azure::Storage::Blobs::PageBlobClient> PageBlobClientTest::m_pageBlobClient;
  std::string PageBlobClientTest::m_blobName;
  Azure::Storage::Blobs::CreatePageBlobOptions PageBlobClientTest::m_blobUploadOptions;
  std::vector<uint8_t> PageBlobClientTest::m_blobContent;

  void PageBlobClientTest::SetUpTestSuite()
  {
    BlobContainerClientTest::SetUpTestSuite();

    m_blobName = RandomString();
    auto pageBlobClient = Azure::Storage::Blobs::PageBlobClient::CreateFromConnectionString(
        StandardStorageConnectionString(), m_containerName, m_blobName);
    m_pageBlobClient
        = std::make_shared<Azure::Storage::Blobs::PageBlobClient>(std::move(pageBlobClient));
    m_blobContent.resize((size_t)1_KB);
    RandomBuffer(reinterpret_cast<char*>(&m_blobContent[0]), m_blobContent.size());
    m_blobUploadOptions.Metadata = {{"key1", "V1"}, {"KEY2", "Value2"}};
    m_blobUploadOptions.Properties.ContentType = "application/x-binary";
    m_blobUploadOptions.Properties.ContentLanguage = "en-US";
    m_blobUploadOptions.Properties.ContentDisposition = "attachment";
    m_blobUploadOptions.Properties.CacheControl = "no-cache";
    m_blobUploadOptions.Properties.ContentEncoding = "identity";
    m_blobUploadOptions.Properties.ContentMD5 = "";
    m_pageBlobClient->Create(m_blobContent.size(), m_blobUploadOptions);
    m_pageBlobClient->UploadPages(
        Azure::Storage::CreateMemoryStream(m_blobContent.data(), m_blobContent.size()), 0);
    m_blobUploadOptions.Properties.ContentMD5 = m_pageBlobClient->GetProperties().ContentMD5;
  }

  void PageBlobClientTest::TearDownTestSuite() { BlobContainerClientTest::TearDownTestSuite(); }

  TEST_F(PageBlobClientTest, CreateDelete)
  {
    auto pageBlobClient = Azure::Storage::Blobs::PageBlobClient::CreateFromConnectionString(
        StandardStorageConnectionString(), m_containerName, RandomString());
    pageBlobClient.Create(0, m_blobUploadOptions);

    pageBlobClient.Delete();
    EXPECT_THROW(pageBlobClient.Delete(), std::runtime_error);
  }

  TEST_F(PageBlobClientTest, Resize)
  {
    auto pageBlobClient = Azure::Storage::Blobs::PageBlobClient::CreateFromConnectionString(
        StandardStorageConnectionString(), m_containerName, RandomString());
    pageBlobClient.Create(0, m_blobUploadOptions);

    EXPECT_EQ(pageBlobClient.GetProperties().ContentLength, 0);
    pageBlobClient.Resize((size_t)2_KB);
    EXPECT_EQ(pageBlobClient.GetProperties().ContentLength, 2_KB);
    pageBlobClient.Resize((size_t)1_KB);
    EXPECT_EQ(pageBlobClient.GetProperties().ContentLength, 1_KB);
  }

  TEST_F(PageBlobClientTest, UploadClear)
  {
    std::vector<uint8_t> blobContent;
    blobContent.resize((size_t)4_KB);
    RandomBuffer(reinterpret_cast<char*>(&blobContent[0]), blobContent.size());

    auto pageBlobClient = Azure::Storage::Blobs::PageBlobClient::CreateFromConnectionString(
        StandardStorageConnectionString(), m_containerName, RandomString());
    pageBlobClient.Create(8_KB, m_blobUploadOptions);
    pageBlobClient.UploadPages(
        Azure::Storage::CreateMemoryStream(blobContent.data(), blobContent.size()), 2_KB);
    // |_|_|x|x|  |x|x|_|_|
    blobContent.insert(blobContent.begin(), (size_t)2_KB, '\x00');
    blobContent.resize((size_t)8_KB, '\x00');
    pageBlobClient.ClearPages(2_KB, 1_KB);
    // |_|_|_|x|  |x|x|_|_|
    std::fill(
        blobContent.begin() + (size_t)2_KB, blobContent.begin() + (size_t)(2_KB + 1_KB), '\x00');

    auto downloadContent = pageBlobClient.Download();
    EXPECT_EQ(ReadBodyStream(downloadContent.BodyStream), blobContent);

    auto pageRanges = pageBlobClient.GetPageRanges();
    EXPECT_TRUE(pageRanges.ClearRanges.empty());
    ASSERT_FALSE(pageRanges.PageRanges.empty());
    EXPECT_EQ(pageRanges.PageRanges[0].Offset, 3_KB);
    EXPECT_EQ(pageRanges.PageRanges[0].Length, 3_KB);

    Azure::Storage::Blobs::GetPageRangesOptions options;
    options.Offset = 4_KB;
    options.Length = 1_KB;
    pageRanges = pageBlobClient.GetPageRanges(options);
    EXPECT_TRUE(pageRanges.ClearRanges.empty());
    ASSERT_FALSE(pageRanges.PageRanges.empty());
    EXPECT_EQ(pageRanges.PageRanges[0].Offset, 4_KB);
    EXPECT_EQ(pageRanges.PageRanges[0].Length, 1_KB);

    auto snapshot = pageBlobClient.CreateSnapshot().Snapshot;
    // |_|_|_|x|  |x|x|_|_| This is what's in snapshot
    blobContent.resize((size_t)1_KB);
    pageBlobClient.UploadPages(
        Azure::Storage::CreateMemoryStream(blobContent.data(), blobContent.size()), 0);
    pageBlobClient.ClearPages(3_KB, 1_KB);
    // |x|_|_|_|  |x|x|_|_|

    options = Azure::Storage::Blobs::GetPageRangesOptions();
    options.PreviousSnapshot = snapshot;
    pageRanges = pageBlobClient.GetPageRanges(options);
    ASSERT_FALSE(pageRanges.ClearRanges.empty());
    ASSERT_FALSE(pageRanges.PageRanges.empty());
    EXPECT_EQ(pageRanges.PageRanges[0].Offset, 0);
    EXPECT_EQ(pageRanges.PageRanges[0].Length, 1_KB);
    EXPECT_EQ(pageRanges.ClearRanges[0].Offset, 3_KB);
    EXPECT_EQ(pageRanges.ClearRanges[0].Length, 1_KB);
  }

  TEST_F(PageBlobClientTest, UploadFromUri)
  {
    // TODO: PutPageFromUri must be authorized with SAS, but we don't have SAS for now.
    /*
    auto pageBlobClient = Azure::Storage::Blobs::PageBlobClient::CreateFromConnectionString(
        StandardStorageConnectionString(), m_containerName, RandomString());
    pageBlobClient.Create(m_blobContent.size(), m_blobUploadOptions);
    pageBlobClient.UploadPagesFromUri(m_pageBlobClient->GetUri(), 0, m_blobContent.size(), 0);
    */
  }

  TEST_F(PageBlobClientTest, StartCopyIncremental)
  {
    // TODO: IncrementalCopyBlob must be authorized with SAS, but we don't have SAS for now.
  }

}}} // namespace Azure::Storage::Test