<?php

/**
 * @file
 * Handles the associating of existing objects for an EAD.
 *
 * This file is part of the Islandora EAD Solution Pack.
 * This file was developed by Bata Library, Trent University and is a modified
 * version of association_upload.form.inc from Drexel University.
 *
 * The Islandora EAD Solution Pack is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * The Islandora EAD Solution Pack is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with The Islandora EAD Solution Pack; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * Defines a file upload form for uploading an associated file.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 * @param AbstractObject $ead
 *   The object representing the EAD.
 * @param string $refid
 *   ID of the level in the EAD content inventory being modified.
 *
 * @return array
 *   The drupal form definition.
 */

function islandora_ead_association_modify_form(array $form, array &$form_state, AbstractObject $ead, $refid) {

  $form['eadid'] = array(
    '#type' => 'hidden',
    '#value' => $ead->id,
  );

  $form['refid'] = array(
    '#type' => 'hidden',
    '#value' => $refid,
  );

  $header = array(
    'level' => t('Level'),
    'info' => t('Info'),
    'association' => t('Association'),
  );

  $xsl = new DOMDocument();
  $xsl->load(drupal_get_path('module', 'islandora_ead') . '/xml/getEadLevels.xsl');

  $xslt = new XSLTProcessor();
  $xslt->importStyleSheet($xsl);

  module_load_include('inc', 'islandora_ead', 'includes/utilities');
  $associations = islandora_ead_retrieve_associations($ead->id);

  $form['associate'] = array(
    '#type' => 'submit',
    '#value' => $associations,
  );

  $ead_dom = new DOMDocument();
  $ead_dom->loadXML($ead['EAD']->content);
  $ead_dom = $xslt->transformToDoc($ead_dom);

  $partsxml = new SimpleXMLElement($ead_dom->saveXML());

  $parts_idx = 1;
  $parts = array();
  
  foreach ($partsxml->entity as $entity) {
    $info = $entity->title;
    if ($entity->id == $refid) {

      if (trim($entity->extent) !== '') {
        $info .= "<br/>$entity->extent";
      }

      if (trim($entity->desc) !== '') {
        $info .= "<br/>$entity->desc";
      }

      $containers = array();

      foreach ($entity->container as $container) {
        if (trim($container->type) !== '') {
          $containers[] = "$container->type: $container->value";
        }
      }

      if ($containers) {
        $info .= "<br/>" . implode(", ", $containers);
      }

      $entity_id = (string) $entity->id;
      $association = '';

      if (array_key_exists($entity_id, $associations)) {
        $association = $associations[$entity_id];
        $assoc_link = "<a href='/islandora/object/$association'>$association</a>";
      }
      else {
        $assoc_link = "No Associations";
      }

      $parts[$parts_idx++] = array(
        'level' => $entity->level,
        'info' => $info,
        'association' => $assoc_link,
      );

      $form['table'] = array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $parts,
        '#empty' => 'No Associations',
      );

      $form['mod_assoc'] = array(
        '#type' => 'textfield',
        '#title' => t('Associated Object'),
        '#default_value' => $association,
        '#autocomplete_path' => 'islandora_ead_object/autocomplete/object',
      );

      $form['exist_assoc'] = array(
        '#type' => 'hidden',
        '#value' => $association,
      );

      $form['submit'] = array(
        '#type' => 'submit',
        '#default_value' => 'Modify',
      );    

      return $form;
    }
  }
}

/**
 * Submit handler, modifies relationships of object.
 */
function islandora_ead_association_modify_form_submit(array $form, array &$form_state) {
  $eadid = $form_state['values']['eadid'];
  $refid = $form_state['values']['refid'];
  $associd_new = $form_state['values']['mod_assoc'];
  $associd = $form_state['values']['exist_assoc'];

  // Modify/create relationships.
  if ($associd_new !== $associd) {
    if ($associd_new !== '') {
      $object = islandora_object_load($associd_new);
      if (!$object) {
         drupal_set_message(t('Error - Object does not exist'), 'error');
      } else {
      $object->relationships->add(FEDORA_RELS_EXT_URI, 'isPartOf', $eadid);
      $object->relationships->add(FEDORA_RELS_EXT_URI, 'eadLevelId', $refid);
    }
    }
    if ($associd !== '' || $associd_new == '') {
      $object = islandora_object_load($associd);
      $object->relationships->remove(FEDORA_RELS_EXT_URI, 'isPartOf', $eadid);
      $object->relationships->remove(FEDORA_RELS_EXT_URI, 'eadLevelId', $refid);
    }
  }

  // Redirect browser to the EAD management of object.
  $form_state['redirect'] = "islandora/object/{$eadid}/manage/ead";
}
