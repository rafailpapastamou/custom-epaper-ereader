#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifndef UNIT_TEST
#include <esp_log.h>
#else
#define ESP_LOGI(tag, args...) \
  printf(args);                \
  printf("\n");
#define ESP_LOGE(tag, args...) \
  printf(args);                \
  printf("\n");
#define ESP_LOGD(tag, args...) \
  printf(args);                \
  printf("\n");
#define ESP_LOGW(tag, args...) \
  printf(args);                \
  printf("\n");
#endif
#include <map>
#include "tinyxml2.h"
#include "atomicZipFile.h"
#include "atomicEpub.h"

static const char *TAG = "EPUB";

std::string normalise_path(const std::string &path);

bool Epub::find_content_opf_file(ZipFile &zip, std::string &content_opf_file)
{
  // open up the meta data to find where the content.opf file lives
  char *meta_info = (char *)zip.read_file_to_memory("META-INF/container.xml");
  if (!meta_info)
  {
    ESP_LOGE(TAG, "Could not find META-INF/container.xml");
    return false;
  }
  // parse the meta data
  tinyxml2::XMLDocument meta_data_doc;
  auto result = meta_data_doc.Parse(meta_info);
  // finished with the data as it's been parsed
  free(meta_info);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Could not parse META-INF/container.xml");
    return false;
  }
  auto container = meta_data_doc.FirstChildElement("container");
  if (!container)
  {
    ESP_LOGE(TAG, "Could not find container element in META-INF/container.xml");
    return false;
  }
  auto rootfiles = container->FirstChildElement("rootfiles");
  if (!rootfiles)
  {
    ESP_LOGE(TAG, "Could not find rootfiles element in META-INF/container.xml");
    return false;
  }
  // find the root file that has the media-type="application/oebps-package+xml"
  auto rootfile = rootfiles->FirstChildElement("rootfile");
  while (rootfile)
  {
    const char *media_type = rootfile->Attribute("media-type");
    if (media_type && strcmp(media_type, "application/oebps-package+xml") == 0)
    {
      const char *full_path = rootfile->Attribute("full-path");
      if (full_path)
      {
        content_opf_file = full_path;
        return true;
      }
    }
    rootfile = rootfile->NextSiblingElement("rootfile");
  }
  ESP_LOGE(TAG, "Could not get path to content.opf file");
  return false;
}

bool Epub::parse_content_opf(ZipFile &zip, std::string &content_opf_file)
{
  // read in the content.opf file and parse it
  char *contents = (char *)zip.read_file_to_memory(content_opf_file.c_str());
  if (!contents)
  {
    ESP_LOGE(TAG, "Could not read %s", content_opf_file.c_str());
    return false;
  }
  // parse the contents
  tinyxml2::XMLDocument doc;
  auto result = doc.Parse(contents);
  free(contents);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Error parsing content.opf - %s", doc.ErrorIDToName(result));
    return false;
  }
  auto package = doc.FirstChildElement("package");
  if (!package)
  {
    ESP_LOGE(TAG, "Could not find package element in content.opf");
    return false;
  }
  // get the metadata - title and cover image
  auto metadata = package->FirstChildElement("metadata");
  if (!metadata)
  {
    ESP_LOGE(TAG, "Missing metadata");
    return false;
  }
  auto title = metadata->FirstChildElement("dc:title");
  if (!title)
  {
    ESP_LOGE(TAG, "Missing title");
    return false;
  }
  const char *title_text = title->GetText();
  m_title = title_text ? title_text : "";

  // EPUB 2 style cover: <meta name="cover" content="item-id"/>
  // (other <meta> elements may have no name attribute, so look at every one)
  const char *cover_item = nullptr;
  for (auto m = metadata->FirstChildElement("meta"); m; m = m->NextSiblingElement("meta"))
  {
    const char *n = m->Attribute("name");
    if (n && strcmp(n, "cover") == 0)
    {
      cover_item = m->Attribute("content");
      break;
    }
  }

  // read the manifest and spine
  // the manifest gives us the names of the files
  // the spine gives us the order of the files
  auto manifest = package->FirstChildElement("manifest");
  if (!manifest)
  {
    ESP_LOGE(TAG, "Missing manifest");
    return false;
  }
  // create a mapping from id to file name
  std::map<std::string, std::string> items;
  for (auto item = manifest->FirstChildElement("item"); item; item = item->NextSiblingElement("item"))
  {
    const char *id_c = item->Attribute("id");
    const char *href_c = item->Attribute("href");
    if (!id_c || !href_c)
    {
      continue;
    }
    std::string item_id = id_c;
    std::string href = normalise_path(m_base_path + href_c);
    const char *type_c = item->Attribute("media-type");
    const char *props_c = item->Attribute("properties");

    // cover image (EPUB 2 meta, then EPUB 3 property)
    if (cover_item && item_id == cover_item)
    {
      m_cover_image_item = href;
    }
    if (props_c && strstr(props_c, "cover-image") && m_cover_image_item.empty())
    {
      m_cover_image_item = href;
    }
    // EPUB 2 table of contents - match on media type as well as id
    if ((type_c && strcmp(type_c, "application/x-dtbncx+xml") == 0) || item_id == "ncx")
    {
      m_toc_ncx_item = href;
    }
    // EPUB 3 table of contents
    if (props_c && strstr(props_c, "nav"))
    {
      m_toc_nav_item = href;
    }
    items[item_id] = href;
  }
  // find the spine
  auto spine = package->FirstChildElement("spine");
  if (!spine)
  {
    ESP_LOGE(TAG, "Missing spine");
    return false;
  }
  // the spine can also name the ncx item
  const char *spine_toc = spine->Attribute("toc");
  if (m_toc_ncx_item.empty() && spine_toc && items.find(spine_toc) != items.end())
  {
    m_toc_ncx_item = items[spine_toc];
  }
  // read the spine
  for (auto itemref = spine->FirstChildElement("itemref"); itemref; itemref = itemref->NextSiblingElement("itemref"))
  {
    const char *id = itemref->Attribute("idref");
    if (id && items.find(id) != items.end())
    {
      m_spine.push_back(std::make_pair(std::string(id), items[id]));
    }
  }
  return true;
}

// walk <navPoint> elements, including nested ones
static void parse_nav_points(tinyxml2::XMLElement *parent, const std::string &base_path,
                             std::vector<EpubTocEntry> &toc, int level)
{
  for (auto navPoint = parent->FirstChildElement("navPoint"); navPoint; navPoint = navPoint->NextSiblingElement("navPoint"))
  {
    auto label = navPoint->FirstChildElement("navLabel");
    auto text = label ? label->FirstChildElement("text") : nullptr;
    auto content = navPoint->FirstChildElement("content");
    if (text && content && content->Attribute("src"))
    {
      const char *t = text->GetText();
      std::string title = t ? t : "";
      std::string href = normalise_path(base_path + content->Attribute("src"));
      // split the href on the # to get the href and the anchor
      std::string anchor;
      size_t pos = href.find('#');
      if (pos != std::string::npos)
      {
        anchor = href.substr(pos + 1);
        href = href.substr(0, pos);
      }
      toc.push_back(EpubTocEntry(title, href, anchor, level));
    }
    parse_nav_points(navPoint, base_path, toc, level + 1);
  }
}

bool Epub::parse_toc_ncx_file(ZipFile &zip)
{
  // the ncx file should have been specified in the content.opf file
  if (m_toc_ncx_item.empty())
  {
    ESP_LOGE(TAG, "No ncx file specified");
    return false;
  }
  ESP_LOGI(TAG, "toc path: %s", m_toc_ncx_item.c_str());

  char *ncx_data = (char *)zip.read_file_to_memory(m_toc_ncx_item.c_str());
  if (!ncx_data)
  {
    ESP_LOGE(TAG, "Could not find %s", m_toc_ncx_item.c_str());
    return false;
  }
  // Parse the Toc contents
  tinyxml2::XMLDocument doc;
  auto result = doc.Parse(ncx_data);
  free(ncx_data);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Error parsing toc %s", doc.ErrorIDToName(result));
    return false;
  }
  auto ncx = doc.FirstChildElement("ncx");
  if (!ncx)
  {
    ESP_LOGE(TAG, "Could not find first child ncx in toc");
    return false;
  }
  auto navMap = ncx->FirstChildElement("navMap");
  if (!navMap)
  {
    ESP_LOGE(TAG, "Could not find navMap child in ncx");
    return false;
  }
  // hrefs in the ncx are relative to the ncx file itself
  std::string ncx_dir = m_toc_ncx_item.substr(0, m_toc_ncx_item.find_last_of('/') + 1);
  parse_nav_points(navMap, ncx_dir, m_toc, 0);
  return !m_toc.empty();
}

// EPUB 3 <ol><li><a href>...</a><ol>...</ol></li></ol>
static void parse_nav_list(tinyxml2::XMLElement *ol, const std::string &nav_dir,
                           std::vector<EpubTocEntry> &toc, int level)
{
  for (auto li = ol->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
  {
    auto a = li->FirstChildElement("a");
    if (a && a->Attribute("href"))
    {
      const char *t = a->GetText();
      std::string title = t ? t : "";
      std::string href = normalise_path(nav_dir + a->Attribute("href"));
      std::string anchor;
      size_t pos = href.find('#');
      if (pos != std::string::npos)
      {
        anchor = href.substr(pos + 1);
        href = href.substr(0, pos);
      }
      toc.push_back(EpubTocEntry(title, href, anchor, level));
    }
    auto sub = li->FirstChildElement("ol");
    if (sub)
    {
      parse_nav_list(sub, nav_dir, toc, level + 1);
    }
  }
}

bool Epub::parse_toc_nav_file(ZipFile &zip)
{
  if (m_toc_nav_item.empty())
  {
    return false;
  }
  ESP_LOGI(TAG, "nav path: %s", m_toc_nav_item.c_str());
  char *data = (char *)zip.read_file_to_memory(m_toc_nav_item.c_str());
  if (!data)
  {
    ESP_LOGE(TAG, "Could not read %s", m_toc_nav_item.c_str());
    return false;
  }
  tinyxml2::XMLDocument doc;
  auto result = doc.Parse(data);
  free(data);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Error parsing nav %s", doc.ErrorIDToName(result));
    return false;
  }
  // some books declare an NCX file as the "nav" item - sniff the root element
  if (doc.FirstChildElement("ncx"))
  {
    m_toc_ncx_item = m_toc_nav_item;
    return parse_toc_ncx_file(zip);
  }
  auto html = doc.FirstChildElement("html");
  auto body = html ? html->FirstChildElement("body") : nullptr;
  if (!body)
  {
    ESP_LOGE(TAG, "nav has no html/body");
    return false;
  }
  std::string nav_dir = m_toc_nav_item.substr(0, m_toc_nav_item.find_last_of('/') + 1);
  // the toc is the <nav epub:type="toc"> element
  for (auto nav = body->FirstChildElement("nav"); nav; nav = nav->NextSiblingElement("nav"))
  {
    const char *type = nav->Attribute("epub:type");
    if (type && strstr(type, "toc"))
    {
      auto ol = nav->FirstChildElement("ol");
      if (ol)
      {
        parse_nav_list(ol, nav_dir, m_toc, 0);
        return !m_toc.empty();
      }
    }
  }
  ESP_LOGE(TAG, "No toc nav found");
  return false;
}

Epub::Epub(const std::string &path) : m_path(path)
{
}

// load in the meta data for the epub file
bool Epub::load()
{
  ZipFile zip(m_path.c_str());
  std::string content_opf_file;
  if (!find_content_opf_file(zip, content_opf_file))
  {
    // NOTE: no esp_restart() here - one unreadable file must not reboot the device
    ESP_LOGE(TAG, "Could not open ePub %s", m_path.c_str());
    return false;
  }
  // get the base path for the content
  m_base_path = content_opf_file.substr(0, content_opf_file.find_last_of('/') + 1);
  if (!parse_content_opf(zip, content_opf_file))
  {
    return false;
  }
  // table of contents: try ncx, then the nav document. A missing toc is not fatal.
  bool have_toc = false;
  if (!m_toc_ncx_item.empty())
  {
    have_toc = parse_toc_ncx_file(zip);
  }
  if (!have_toc)
  {
    m_toc.clear();
    have_toc = parse_toc_nav_file(zip);
  }
  if (!have_toc)
  {
    m_toc.clear();
    ESP_LOGW(TAG, "No table of contents found in %s", m_path.c_str());
  }
  return true;
}

const std::string &Epub::get_title()
{
  return m_title;
}

const std::string &Epub::get_cover_image_item()
{
  return m_cover_image_item;
}

std::string normalise_path(const std::string &path)
{
  std::vector<std::string> components;
  std::string component;
  for (auto c : path)
  {
    if (c == '/')
    {
      if (!component.empty())
      {
        if (component == "..")
        {
          if (!components.empty())
          {
            components.pop_back();
          }
        }
        else
        {
          components.push_back(component);
        }
        component.clear();
      }
    }
    else
    {
      component += c;
    }
  }
  if (!component.empty())
  {
    components.push_back(component);
  }
  std::string result;
  for (auto &component : components)
  {
    if (result.size() > 0)
    {
      result += "/";
    }
    result += component;
  }
  return result;
}

uint8_t *Epub::get_item_contents(const std::string &item_href, size_t *size)
{
  ZipFile zip(m_path.c_str());
  std::string path = normalise_path(item_href);
  auto content = zip.read_file_to_memory(path.c_str(), size);
  if (!content)
  {
    ESP_LOGE(TAG, "Failed to read item %s", path.c_str());
    return nullptr;
  }
  return content;
}

int Epub::get_spine_items_count()
{
  return m_spine.size();
}

std::string &Epub::get_spine_item(int spine_index)
{
  try
  {
    return m_spine.at(spine_index).second;
  }
  catch (const std::out_of_range &oor)
  {

    ESP_LOGI(TAG, "get_spine_item index:%d is out_of_range", spine_index);
    spine_index = 0;
  }
  // if exception is catched return last page
  return m_spine.at(spine_index).second;
}

EpubTocEntry &Epub::get_toc_item(int toc_index)
{
  return m_toc[toc_index];
}

int Epub::get_toc_items_count()
{
  return m_toc.size();
}

// work out the section index for a toc index
int Epub::get_spine_index_for_toc_index(int toc_index)
{
  // the toc entry should have an href that matches the spine item
  // so we can find the spine index by looking for the href
  for (int i = 0; i < (int)m_spine.size(); i++)
  {
    if (m_spine[i].second == m_toc[toc_index].href)
    {
      return i;
    }
  }
  ESP_LOGI(TAG, "Section not found");
  // not found - default to the start of the book
  return 0;
}